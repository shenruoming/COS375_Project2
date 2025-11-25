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
        pipelineInfo.wbInst = simulator->simWB(pipelineInfo.memInst);
        // forward to rs2 of load if needed: no stall for load-store (WB-> MEM)
        if (pipelineInfo.wbInst.opcode == OP_LOAD && pipelineInfo.exInst.opcode == OP_STORE && pipelineInfo.wbInst.rd == pipelineInfo.exInst.rs2) {
            std::cout << "should NOT be here: "  << PC << std::endl;
            pipelineInfo.exInst.op2Val = pipelineInfo.wbInst.memResult;
        }
        pipelineInfo.memInst = simulator->simMEM(pipelineInfo.exInst);

        
        // applies to load-use with stalling
        // load-use for R-type (load first, then use as an input register)
        if (pipelineInfo.memInst.opcode == OP_LOAD && (pipelineInfo.idInst.opcode != OP_STORE) &&
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
            // NOP in between load and store, 
            if (pipelineInfo.wbInst.opcode == OP_LOAD 
                && pipelineInfo.idInst.opcode == OP_STORE 
                && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs1) {
                std::cout << "forwarding from WB to ID before executing ID for rs1 (load->store): "  << PC << std::endl;
                pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.memResult;
            }
            // also update dest reg of store if dependent on load
            if (pipelineInfo.wbInst.opcode == OP_LOAD 
                && pipelineInfo.idInst.opcode == OP_STORE 
                && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs2) {
                std::cout << "forwarding from WB to ID before executing ID for rs2 (load->store): "  << PC << std::endl;
                pipelineInfo.idInst.op2Val = pipelineInfo.wbInst.memResult;
            }

            if (pipelineInfo.wbInst.opcode == OP_LOAD 
                && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs1) {
                std::cout << "forwarding from WB to ID before executing ID for rs1 (load->add): "  << PC << std::endl;
                pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.memResult;
            }
            if (pipelineInfo.wbInst.opcode == OP_LOAD 
                && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs2) {
                std::cout << "forwarding from WB to ID before executing ID for rs2 (load->add): "  << PC << std::endl;
                pipelineInfo.idInst.op2Val = pipelineInfo.wbInst.memResult;
            }


            // MAKE SURE HERE THAT WB INSTR OR MEM INSTR ISNT A BRANCH 
            // checking if register we need for execute was just calculated add -> smth -> add
            if (pipelineInfo.wbInst.opcode != OP_LOAD && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs1) {
                std::cout << "forward from wb to ex for rs1: "  << pipelineInfo.wbInst.arithResult << std::endl;
                pipelineInfo.idInst.op1Val = pipelineInfo.wbInst.arithResult;
            }
            if (pipelineInfo.wbInst.opcode != OP_LOAD && pipelineInfo.wbInst.rd == pipelineInfo.idInst.rs2) {
                std::cout << "forward from wb to ex for rs2: "  << pipelineInfo.wbInst.arithResult << std::endl;
                // std::cout << "should be here at t4 for t3: "  << pipelineInfo.wbInst.arithResult << std::endl;
                pipelineInfo.idInst.op2Val = pipelineInfo.wbInst.arithResult;
            }
            // R-type -> R-type, store, and load hopefully
            // checking if register we need for execute was just calculated add -> add
            if (pipelineInfo.memInst.opcode != OP_LOAD && pipelineInfo.memInst.rd == pipelineInfo.idInst.rs1) {
                std::cout << "forward from mem to ex for rs1: "  << pipelineInfo.memInst.arithResult << std::endl;
                pipelineInfo.idInst.op1Val = pipelineInfo.memInst.arithResult;
            }
            if (pipelineInfo.memInst.opcode != OP_LOAD && pipelineInfo.memInst.rd == pipelineInfo.idInst.rs2) {
                std::cout << "forward from mem to ex for rs2: "  << pipelineInfo.memInst.arithResult << std::endl;
                pipelineInfo.idInst.op2Val = pipelineInfo.memInst.arithResult;
            }

            if (pipelineInfo.idInst.opcode == OP_BRANCH 
                && (pipelineInfo.memInst.opcode == OP_INT || pipelineInfo.exInst.opcode == OP_INTIMM 
                    || pipelineInfo.exInst.opcode == OP_INTIMMW || pipelineInfo.exInst.opcode == OP_INTW || pipelineInfo.exInst.opcode == OP_AUIPC)
                && (pipelineInfo.idInst.rs1 == pipelineInfo.exInst.rd || pipelineInfo.idInst.rs2 == pipelineInfo.exInst.rd)) {

                if (pipelineInfo.idInst.rs1 == pipelineInfo.exInst.rd) {
                    pipelineInfo.idInst.op1Val = pipelineInfo.exInst.arithResult;
                }
                if (pipelineInfo.idInst.rs2 == pipelineInfo.exInst.rd) {
                    pipelineInfo.idInst.op2Val = pipelineInfo.exInst.arithResult;
                }
                pipelineInfo.idInst = simulator->simNextPCResolution(pipelineInfo.idInst);
                pipelineInfo.exInst = nop(BUBBLE);
                pipelineInfo.idInst = pipelineInfo.idInst;
                pipelineInfo.ifInst.status = SPECULATIVE;
            } else {
                pipelineInfo.exInst = simulator->simEX(pipelineInfo.idInst);
                if (pipelineInfo.idInst.nextPC != pipelineInfo.ifInst.PC) {
                    pipelineInfo.idInst = nop(SQUASHED);
                    PC = pipelineInfo.idInst.nextPC;
                } else {
                    pipelineInfo.ifInst.status = NORMAL;
                    pipelineInfo.idInst = simulator->simID(pipelineInfo.ifInst);
                }
                pipelineInfo.ifInst = simulator->simIF(PC);
                PC = PC + 4;
                
            }

            // pipelineInfo.exInst = simulator->simEX(pipelineInfo.idInst);
            // if we are storing a register that we just loaded to (no stall)
            // if (pipelineInfo.memInst.opcode == OP_LOAD && pipelineInfo.exInst.opcode == OP_STORE && pipelineInfo.memInst.rd == pipelineInfo.exInst.rs2) {
            //     pipelineInfo.exInst.op2Val = pipelineInfo.memInst.memResult;
            // }
            // std::cout << "result of ex: "  << pipelineInfo.exInst.arithResult << std::endl;
    
            // pipelineInfo.idInst = simulator->simID(pipelineInfo.ifInst);
            
            // std::cout << "next pc: "  << PC << std::endl;

            // pipelineInfo.ifInst = simulator->simIF(PC);
            // // have to update for branching
            // PC = PC + 4;

        }
        
        
        // later add goes into execute and stays there until value is available
        // WB Check for halt instruction
        if (pipelineInfo.wbInst.isHalt) {
            status = HALT;
            break;
        }
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
    Status status;
    while (true) {
        status = static_cast<Status>(runCycles(1));
        if (status == HALT) break;
    }
    return status;
}

// dump the state of the simulator
Status finalizeSimulator() {
    simulator->dumpRegMem(output);
    SimulationStats stats{simulator->getDin(),  cycleCount, 0, 0, 0, 0, 0};  // TODO incomplete implementation
    dumpSimStats(stats, output);
    return SUCCESS;
}
