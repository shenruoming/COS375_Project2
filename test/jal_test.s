addi a4, zero, 30
addi a5, zero, 240
sd a4, 0(a5)
ld t5, 0(a5)
jal t5\6, branch # change to jal
addi t2, zero, 25

branch:
    addi t1, zero, 8

.word 0xfeedfeed