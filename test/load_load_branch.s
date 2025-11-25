addi a4, zero, 30
addi a5, zero, 240
sd a4, 0(a5)
ld t6, 0(a5)
ld t5, 0(a5)
beq t5, a6, branch
addi t2, zero, 25

branch:
    addi t1, zero, 8

.word 0xfeedfeed
