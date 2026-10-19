#include <mmu.h>
#include <memlayout.h>
.section .text,"ax",%progbits
.globl kern_entry
kern_entry:
    # lab2 function1 2413460: keep a0 (hart ID) and a1 (DTB PA) intact.
    .option push
    .option norelax
    la t0, boot_page_table_sv39
    .option pop
    srli t0, t0, 12
    li t1, 8 << 60
    or t0, t0, t1
    csrw satp, t0
    sfence.vma
    # A temporary identity leaf keeps the instruction stream valid here.
    lui sp, %hi(bootstacktop)
    addi sp, sp, %lo(bootstacktop)
    lui t0, %hi(kern_init)
    addi t0, t0, %lo(kern_init)
    jr t0
.section .data
.align PGSHIFT
.globl bootstack
bootstack:
    .space KSTACKSIZE
.globl bootstacktop
bootstacktop:
.align PGSHIFT
.globl boot_page_table_sv39
boot_page_table_sv39:
    .zero 8 * 2
    .quad (0x80000 << 10) | 0xcf
    .zero 8 * 508
    .quad (0x80000 << 10) | 0xcf
