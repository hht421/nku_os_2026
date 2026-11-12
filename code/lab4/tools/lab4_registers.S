#include <riscv.h>
.text
.globl lab4_register_probe
lab4_register_probe:
    addi sp, sp, -128
    STORE s0, 0(sp)
    STORE s1, 8(sp)
    STORE s2, 16(sp)
    STORE s3, 24(sp)
    STORE s4, 32(sp)
    STORE s5, 40(sp)
    STORE s6, 48(sp)
    STORE s7, 56(sp)
    STORE s8, 64(sp)
    STORE s9, 72(sp)
    STORE s10, 80(sp)
    STORE s11, 88(sp)
    STORE ra, 96(sp)
    STORE sp, 104(sp)
    li t0, 3
    STORE t0, 112(sp)
    li s0, 4660
    li s1, 4661
    li s2, 4662
    li s3, 4663
    li s4, 4664
    li s5, 4665
    li s6, 4666
    li s7, 4667
    li s8, 4668
    li s9, 4669
    li s10, 4670
    li s11, 4671
1:
    call schedule
    LOAD t0, 104(sp)
    bne sp, t0, 3f
    li t0, 4660
    bne s0, t0, 3f
    li t0, 4661
    bne s1, t0, 3f
    li t0, 4662
    bne s2, t0, 3f
    li t0, 4663
    bne s3, t0, 3f
    li t0, 4664
    bne s4, t0, 3f
    li t0, 4665
    bne s5, t0, 3f
    li t0, 4666
    bne s6, t0, 3f
    li t0, 4667
    bne s7, t0, 3f
    li t0, 4668
    bne s8, t0, 3f
    li t0, 4669
    bne s9, t0, 3f
    li t0, 4670
    bne s10, t0, 3f
    li t0, 4671
    bne s11, t0, 3f
    LOAD t0, 112(sp)
    addi t0, t0, -1
    STORE t0, 112(sp)
    bnez t0, 1b
    li a0, 0
    j 2f
3:
    li a0, 1
2:
    LOAD s0, 0(sp)
    LOAD s1, 8(sp)
    LOAD s2, 16(sp)
    LOAD s3, 24(sp)
    LOAD s4, 32(sp)
    LOAD s5, 40(sp)
    LOAD s6, 48(sp)
    LOAD s7, 56(sp)
    LOAD s8, 64(sp)
    LOAD s9, 72(sp)
    LOAD s10, 80(sp)
    LOAD s11, 88(sp)
    LOAD ra, 96(sp)
    addi sp, sp, 128
    ret
