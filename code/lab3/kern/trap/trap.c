#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <sbi.h>
#include <stdio.h>
#include <trap.h>

#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
 */
void idt_init(void) {

    extern void __alltraps(void);
    /* Set sup0 scratch register to 0, indicating to exception vector
       that we are presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trapframe *tf) {
    return (tf->status & SSTATUS_SPP) != 0;
}

void print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr) {
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

//lab3 function2 2413456
/* interrupt_handler 与 exception_handler 的实现（本占位处为 lab3 function2）
 * interrupt_handler（时钟中断/软中断/外部中断）：2413456
 * exception_handler（非法指令/断点/其他异常兜底）：2413460 */

// 累计输出 TICK_NUM 统计信息的次数达到 SHUTDOWN_AFTER 后调用 sbi_shutdown() 关机
#define SHUTDOWN_AFTER 10

static size_t shutdown_count = 0;

/* interrupt_handler - 处理各类中断；scause 最高位为 1 时由 trap_dispatch 进入这里 */
void interrupt_handler(struct trapframe *tf) {
    // 清除最高位（中断标志）得到具体的中断编号
    uintptr_t code = (uintptr_t)tf->cause & ~((uintptr_t)1 << 63);
    switch (code) {
    case IRQ_S_TIMER:
        // 时钟中断：必须重新设置下一次时钟事件，否则只能收到一次中断
        clock_set_next_event();
        ticks++;
        if (ticks % TICK_NUM == 0) {
            print_ticks();
            shutdown_count++;
            if (shutdown_count >= SHUTDOWN_AFTER) {
                sbi_shutdown();
            }
        }
        break;
    case IRQ_S_SOFT:
        cprintf("Supervisor software interrupt\n");
        break;
    case IRQ_S_EXT:
        cprintf("Supervisor external interrupt\n");
        break;
    default:
        // 无法识别的中断：输出提示并用 print_trapframe 辅助调试
        cprintf("Unknown interrupt: cause %d\n", code);
        print_trapframe(tf);
        break;
    }
}

/* exception_handler - 处理各类异常；scause 最高位为 0 时由 trap_dispatch 进入这里 */

/* 读取 epc 处指令编码的低两位判断指令长度：
 * 0b11 为 32 位标准指令（4 字节），否则为 16 位压缩指令（2 字节，RVC 扩展）。
 * 例如 ebreak 在开启压缩扩展后会被汇编为 2 字节的 c.ebreak，而自定义非法指令
 * .word 0x0000007b 是 4 字节——不能对所有异常假定同一偏移量。 */
static uintptr_t instruction_length(uintptr_t pc) {
    uint16_t inst = *(volatile uint16_t *)pc;
    return (inst & 0x3) == 0x3 ? 4 : 2;
}

void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
    case CAUSE_ILLEGAL_INSTRUCTION:
        // 非法指令：按实际编码长度前移返回地址，避免再次触发同一异常形成死循环
        cprintf("Illegal instruction at 0x%08x\n", tf->epc);
        tf->epc += instruction_length(tf->epc);
        break;
    case CAUSE_BREAKPOINT:
        // 断点异常：同样按实际编码长度前移（c.ebreak 为 2 字节）
        cprintf("Breakpoint at 0x%08x\n", tf->epc);
        tf->epc += instruction_length(tf->epc);
        break;
    default:
        // 实验未要求具体恢复逻辑的异常：print_trapframe 兜底输出
        print_trapframe(tf);
        break;
    }
}


static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        // interrupts
        interrupt_handler(tf);
    } else {
        // exceptions
        exception_handler(tf);
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}
