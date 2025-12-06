#include "cycle.h"

#include <iostream>
#include <memory>
#include <string>
#include <stdio.h>

#include "Utilities.h"
#include "cache.h"
#include "simulator.h"

static Simulator* simulator = nullptr;
static Cache* iCache = nullptr;
static Cache* dCache = nullptr;
static std::string output;
static uint64_t cycleCount = 0;

static uint64_t PC = 0;

static bool reachedIllegal = false;
static int numDCacheStalls = 0;
static int numICacheStalls = 0;
static bool inBranch = false;
static bool changedExceptionControl = false;
static uint64_t correctBranchPC = 0;

/**TODO: Implement pipeline simulation for the RISCV machine in this file.
 * A basic template is provided below that doesn't account for any hazards.
 */

Simulator::Instruction nop(StageStatus status) {
    Simulator::Instruction nop;
    nop.instruction = 0x00000013;
    nop.isLegal = true;
    nop.isNop = true;
    nop.status = status;
    return nop;
}

static struct PipelineInfo {
    Simulator::Instruction ifInst = nop(IDLE);
    Simulator::Instruction idInst = nop(IDLE);
    Simulator::Instruction exInst = nop(IDLE);
    Simulator::Instruction memInst = nop(IDLE);
    Simulator::Instruction wbInst = nop(IDLE);
} pipelineInfo;


// initialize the simulator
Status initSimulator(CacheConfig& iCacheConfig, CacheConfig& dCacheConfig, MemoryStore* mem,
                     const std::string& output_name) {
    output = output_name;
    simulator = new Simulator();
    simulator->setMemory(mem);
    iCache = new Cache(iCacheConfig, I_CACHE);
    dCache = new Cache(dCacheConfig, D_CACHE);
    return SUCCESS;
}

// run the simulator for a certain number of cycles
// return SUCCESS if reaching desired cycles.
// return HALT if the simulator halts on 0xfeedfeed

Status runCycles(uint64_t cycles) {
    uint64_t count = 0;
    auto status = SUCCESS;
    PipeState pipeState = {
        0,
    };


    while (cycles == 0 || count < cycles) {

        pipeState.cycle = cycleCount;
        count++;
        cycleCount++;
 
        pipelineInfo.wbInst = nop(BUBBLE);

        // simulate D-cache stalls
        if (numDCacheStalls > 0) {
            numDCacheStalls -= 1;
            if (numICacheStalls > 0) {
                numICacheStalls -= 1;
            }
            break;
        } else if (numICacheStalls > 0) {
            numICacheStalls -= 1;
        }

        pipelineInfo.wbInst = simulator->simWB(pipelineInfo.memInst);
        // forward to rs2 of load if needed: no stall for load-store (WB-> MEM)
        if (pipelineInfo.wbInst.opcode == OP_LOAD && pipelineInfo.exInst.opcode == OP_STORE && pipelineInfo.wbInst.rd == pipelineInfo.exInst.rs2) {
            pipelineInfo.exInst.op2Val = pipelineInfo.wbInst.memResult;
        }
        pipelineInfo.memInst = simulator->simMEM(pipelineInfo.exInst);

        // simulate D-cache
        if (pipelineInfo.memInst.opcode == OP_LOAD || pipelineInfo.memInst.opcode == OP_STORE) {
            CacheOperation op = CACHE_READ;
            if (pipelineInfo.memInst.opcode == OP_STORE) {
                op = CACHE_WRITE;
            }
            bool hit = dCache->access(pipelineInfo.memInst.memAddress, op);
            if (!hit) {
                numDCacheStalls = dCache->config.missLatency;
            }
        }
        
        // applies to load-use with stalling
        // load-use for R-type (load first, then use as an input register)
        if (pipelineInfo.memInst.opcode == OP_LOAD && (pipelineInfo.idInst.opcode != OP_STORE) && pipelineInfo.idInst.opcode != OP_BRANCH &&
            (pipelineInfo.idInst.rs1 == pipelineInfo.memInst.rd || pipelineInfo.idInst.rs2 == pipelineInfo.memInst.rd)) {
            pipelineInfo.ifInst = pipelineInfo.ifInst;
            pipelineInfo.idInst = pipelineInfo.idInst;

            // forward to ID instruction before we lose the WB instruction
            if (pipelineInfo.wbInst.opcode == OP_LOAD) {
                if (pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs1) {
                    pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.memResult;
                }
                if (pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs2) {
                    pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.memResult;
                }
            } else {
                // for case where we need WB instruct arith result for addition in instruc
                if (pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs1) {
                    pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.arithResult;
                }
                if (pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs2) {
                    pipelineInfo.idInst.op2Val = pipelineInfo.wbInst.arithResult;
                }
            }
        
            pipelineInfo.exInst = nop(BUBBLE);
        // load-use for store (rs1 is in conflict) if we are storing at a register we just loaded the value of.
        } else if (pipelineInfo.memInst.opcode == OP_LOAD && pipelineInfo.idInst.opcode == OP_STORE &&
            pipelineInfo.idInst.rs1 == pipelineInfo.memInst.rd) {
            pipelineInfo.ifInst = pipelineInfo.ifInst;
            pipelineInfo.idInst = pipelineInfo.idInst;
            pipelineInfo.exInst = nop(BUBBLE);
        } else {
            // delete maybe: "refresh" id instruction registers in case of long cache stalls
            // pipelineInfo.idInst = simulator->simID(pipelineInfo.idInst); 

            // NOP in between load and store, 
            if (pipelineInfo.wbInst.opcode == OP_LOAD 
                && pipelineInfo.idInst.opcode == OP_STORE 
                && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs1) {
                // std::cout << "forwarding from WB to ID before executing ID for rs1 (load->store): "  << PC << std::endl;
                pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.memResult;
            }
            // also update dest reg of store if dependent on load
            if (pipelineInfo.wbInst.opcode == OP_LOAD 
                && pipelineInfo.idInst.opcode == OP_STORE 
                && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs2) {
                // std::cout << "forwarding from WB to ID before executing ID for rs2 (load->store): "  << PC << std::endl;
                pipelineInfo.idInst.op2Val = pipelineInfo.wbInst.memResult;
            }

            if (pipelineInfo.wbInst.opcode == OP_LOAD 
                && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs1) {
                // std::cout << "forwarding from WB to ID before executing ID for rs1 (load->add): "  << PC << std::endl;
                pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.memResult;
            }
            if (pipelineInfo.wbInst.opcode == OP_LOAD 
                && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs2) {
                // std::cout << "forwarding from WB to ID before executing ID for rs2 (load->add): "  << PC << std::endl;
                pipelineInfo.idInst.op2Val = pipelineInfo.wbInst.memResult;
            }


            // MAKE SURE HERE THAT WB INSTR OR MEM INSTR ISNT A BRANCH 
            // checking if register we need for execute was just calculated add -> smth -> add
            if (pipelineInfo.wbInst.opcode != OP_LOAD && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs1) {
                // std::cout << "forward from wb to ex for rs1: "  << pipelineInfo.wbInst.arithResult << std::endl;
                pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.arithResult;
            }
            if (pipelineInfo.wbInst.opcode != OP_LOAD && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs2) {
                // std::cout << "forward from wb to ex for rs2: "  << pipelineInfo.wbInst.arithResult << std::endl;
                // std::cout << "should be here at t4 for t3: "  << pipelineInfo.wbInst.arithResult << std::endl;
                pipelineInfo.idInst.op2Val = pipelineInfo.wbInst.arithResult;
            }
            // R-type -> R-type, store, and load hopefully
            // checking if register we need for execute was just calculated add -> add
            if (pipelineInfo.memInst.opcode != OP_LOAD && pipelineInfo.memInst.rd == pipelineInfo.idInst.rs1) {
                // std::cout << "forward from mem to ex for rs1: "  << pipelineInfo.memInst.arithResult << std::endl;
                pipelineInfo.idInst.op1Val = pipelineInfo.memInst.arithResult;
            }
            if (pipelineInfo.memInst.opcode != OP_LOAD && pipelineInfo.memInst.rd == pipelineInfo.idInst.rs2) {
                // std::cout << "forward from mem to ex for rs2: "  << pipelineInfo.memInst.arithResult << std::endl;
                pipelineInfo.idInst.op2Val = pipelineInfo.memInst.arithResult;
            }
            // one cycle arith branch stal
            if (pipelineInfo.idInst.opcode == OP_BRANCH 
                && (pipelineInfo.exInst.opcode == OP_INT || pipelineInfo.exInst.opcode == OP_INTIMM 
                    || pipelineInfo.exInst.opcode == OP_INTIMMW || pipelineInfo.exInst.opcode == OP_INTW || pipelineInfo.exInst.opcode == OP_AUIPC)
                && (pipelineInfo.idInst.rs1 == pipelineInfo.exInst.rd || pipelineInfo.idInst.rs2 == pipelineInfo.exInst.rd)) {

                if (pipelineInfo.idInst.rs1 == pipelineInfo.exInst.rd) {
                    pipelineInfo.idInst.op1Val = pipelineInfo.exInst.arithResult;
                }
                if (pipelineInfo.idInst.rs2 == pipelineInfo.exInst.rd) {
                    pipelineInfo.idInst.op2Val = pipelineInfo.exInst.arithResult;
                }
                pipelineInfo.exInst = nop(BUBBLE);
                pipelineInfo.idInst = simulator->simNextPCResolution(pipelineInfo.idInst);
            // two cycle load branch stall
            } else if (pipelineInfo.idInst.opcode == OP_BRANCH 
                && (pipelineInfo.exInst.opcode == OP_LOAD)
                && (pipelineInfo.idInst.rs1 == pipelineInfo.exInst.rd || pipelineInfo.idInst.rs2 == pipelineInfo.exInst.rd)) {
                // should 
                if (pipelineInfo.idInst.rs1 == pipelineInfo.exInst.rd) {
                    pipelineInfo.idInst.op1Val = pipelineInfo.exInst.arithResult;
                }
                if (pipelineInfo.idInst.rs2 == pipelineInfo.exInst.rd) {
                    pipelineInfo.idInst.op2Val = pipelineInfo.exInst.arithResult;
                }
                pipelineInfo.exInst = nop(BUBBLE);
                // pipelineInfo.idInst = simulator->simNextPCResolution(pipelineInfo.idInst);
            } else if (pipelineInfo.idInst.opcode == OP_BRANCH 
                && (pipelineInfo.wbInst.opcode == OP_LOAD)
                && (pipelineInfo.idInst.rs1 == pipelineInfo.wbInst.rd || pipelineInfo.idInst.rs2 == pipelineInfo.wbInst.rd)) {

                pipelineInfo.idInst = simulator->simID(pipelineInfo.idInst);
                // should we call operand collection here again so that we have the most up to date input register
                if (pipelineInfo.idInst.rs1 == pipelineInfo.wbInst.rd) {
                    pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.memResult;
                }
                if (pipelineInfo.idInst.rs2 == pipelineInfo.wbInst.rd) {
                    pipelineInfo.idInst.op2Val = pipelineInfo.wbInst.memResult;
                }
                pipelineInfo.exInst = nop(BUBBLE);
                // "refresh" the branch's next PC
                pipelineInfo.idInst = simulator->simNextPCResolution(pipelineInfo.idInst);
            } else {
                // exception handling for illegal instruction
                if (reachedIllegal) {
                    pipelineInfo.idInst = nop(SQUASHED);
                    // reachedIllegal = true;
                    // if (!changedExceptionControl) {
                    //     changedExceptionControl = true;
                    //     numICacheStalls = 0;
                    // }
                }

                pipelineInfo.exInst = simulator->simEX(pipelineInfo.idInst);

                if (numICacheStalls > 0) {
                    pipelineInfo.idInst = nop(BUBBLE);
                    break;
                }

                if (inBranch) {
                    simulator->simNextPCResolution(pipelineInfo.idInst);
                    correctBranchPC = pipelineInfo.idInst.nextPC;
                }
                
                if (inBranch && correctBranchPC != pipelineInfo.ifInst.PC) {
                    std::cout << "wrong branch prediction, new PC is: "  << pipelineInfo.idInst.nextPC << std::endl;
                    PC = correctBranchPC;
                    pipelineInfo.idInst = nop(SQUASHED);
                } else {
                    std::cout << "correct branch prediction, new PC is: "  << pipelineInfo.idInst.nextPC << std::endl;
                    if (!pipelineInfo.ifInst.isNop) {
                        pipelineInfo.ifInst.status = NORMAL;
                    }

                    pipelineInfo.idInst = simulator->simID(pipelineInfo.ifInst);
                    // after raising an illegal instruction exception, squash future instructions
                    if (reachedIllegal && !pipelineInfo.idInst.isHalt) {
                        pipelineInfo.idInst = nop(SQUASHED);
                    }
                }
                inBranch = false;
                pipelineInfo.ifInst = simulator->simIF(PC);
                if (pipelineInfo.idInst.opcode == OP_BRANCH) {
                    pipelineInfo.ifInst.status = SPECULATIVE;
                    inBranch = true;
                }

                // simulate ICache
                bool iHit = iCache->access(pipelineInfo.ifInst.PC, CACHE_READ);
                if (!iHit) {
                    numICacheStalls = iCache->config.missLatency + 1;
                }
                PC = PC + 4;
                // exception handling: jump to address 0x8000 after reaching first illegal instruction
                if (!pipelineInfo.idInst.isLegal) {
                    PC = 0x8000;
                    reachedIllegal = true;
                    numICacheStalls = 0;
                }
                
            }

        }
        
        
        // later add goes into execute and stays there until value is available
        // WB Check for halt instruction
        if (pipelineInfo.wbInst.isHalt) {
            status = HALT;
            break;
        }
    }
    if (pipelineInfo.wbInst.isHalt) {
        status = HALT;
    }

    
    pipeState.ifPC = pipelineInfo.ifInst.PC;
    pipeState.ifStatus = pipelineInfo.ifInst.status;
    pipeState.idInstr = pipelineInfo.idInst.instruction;
    pipeState.idStatus = pipelineInfo.idInst.status;
    pipeState.exInstr = pipelineInfo.exInst.instruction;
    pipeState.exStatus = pipelineInfo.exInst.status;
    pipeState.memInstr = pipelineInfo.memInst.instruction;
    pipeState.memStatus = pipelineInfo.memInst.status;
    pipeState.wbInstr = pipelineInfo.wbInst.instruction;
    pipeState.wbStatus = pipelineInfo.wbInst.status;
    dumpPipeState(pipeState, output);
    return status;
}

// run till halt (call runCycles() with cycles == 1 each time) until
// status tells you to HALT or ERROR out
Status runTillHalt() {
    uint64_t addresses[18] = {0x0, 0x4, 0x8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24,0x28,0x2c, 0x30, 0x34, 0x38, 0x0000F0001,  0x000FF0001, 0x0};
    for (int i = 0; i < 18; i++) {
        iCache->access(addresses[i], CACHE_READ);
    }
    return SUCCESS;
    // Status status;
    // while (true) {
    //     status = static_cast<Status>(runCycles(1));
    //     if (status == HALT) break;
    // }
    // return status;
}

// dump the state of the simulator
Status finalizeSimulator() {
    simulator->dumpRegMem(output);
    SimulationStats stats{simulator->getDin(),  cycleCount, 0, 0, 0, 0, 0};  // TODO incomplete implementation
    dumpSimStats(stats, output);
    return SUCCESS;
}
