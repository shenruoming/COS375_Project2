addi a4, zero, 30
addi s6, zero, 8
addi a5, zero, 240
addi s3, zero, 8
sd s6, 0(a5)
ld t5, 0(a5)
ld s3, 0(a5)
jalr t6, 28(s3)
addi t2, zero, 25

branch:
    addi s4, zero, 1
    addi s5, zero, 1
    addi t1, zero, 8

.word 0xfeedfeed