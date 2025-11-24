addi t1, zero, 1
addi a4, zero, 30
addi a5, zero, 240
sd a4, 0(a5)
addi t2, t1, 5
addi t3, t1, 5
add  t4, t2, t3
add t4, t4, t4
ld t5, 216(t4)
sd t5, 219(t1)
addi t1, zero, 1
addi a4, zero, 30
addi a5, zero, 240
sd a4, 0(a5)
addi t2, t1, 5
addi t3, t1, 5
add  t4, t2, t3
add t4, t4, t4
ld t5, 216(t4)
sd t5, 219(t1)
sd t5, 129(t1)
.word 0xfeedfeed