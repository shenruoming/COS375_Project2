addi a4, zero, 30
addi a5, zero, 240
addi s3, zero, 8
sd a4, 0(a5)
ld t5, 0(a5)
jalr t6, branch(s3)
addi t2, zero, 25

branch:
    addi s4, zero, 1
    addi s5, zero, 1
    addi t1, zero, 8

.word 0xfeedfeed