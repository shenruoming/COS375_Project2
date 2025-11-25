addi t1, zero, 1
addi t2, t1, 5
addi t3, t1, 5
add  t4, t2, t3
add t4, t4, t4
bne t4, zero, branch
addi t5, zero, 6
branch:
    addi t6, zero, 7
.word 0xfeedfeed