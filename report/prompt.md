# Lab 3 提示词文档：四段式 Spec 与 Prompt

本实验延续 Lab 2 采用的“四段式”提示词规范（[PROMPT] / [RELY] / [GUARANTEE] / [SPECIFICATION]）。
Lab 3 官方框架中所有需要完成的位置均以 `lab3`/`lab2` 注释标出，以下提示词按官方框架的实际占位组织。

## 0. Lab 2 代码移植：填充官方 Lab 3 中的 lab2 占位

[PROMPT]

**任务**：官方 Lab 3 框架（`code/lab3/`）依赖实验二成果。请将实验二已验收的代码移植到官方骨架中标有
`//lab2 请补充你在lab2中的代码` 的两处：`kern/mm/best_fit_pmm.c`（Best-Fit 分配器函数与
`best_fit_pmm_manager` 定义）和 `kern/mm/pmm.c`（`page_init()`），并把 `init_pmm_manager()`
接入 Best-Fit，使官方框架能够编译并通过 `best_fit_check()`。

**操作要求**：只允许在官方标记的占位处及其必要关联处填写代码；官方提供的 `basic_check()`、
`best_fit_check()` 一字不动；保留官方 `//lab2` 注释；不得引入实验二自建结构
（`buddy_pmm.c`、`pmm_layout.c`、`PMM_KIND` 开关等），布局计算改为在 `page_init()` 内联完成。

**输出要求**：代码符合 `pmm.h` 中 `struct pmm_manager` 接口约定；`pages`、`npage`、`va_pa_offset`
语义与官方 `memlayout.h`、`pmm.h` 一致；移植完成后官方 `make` 能编译出 `bin/ucore.img` 且
`check_alloc_page() succeeded!` 正常输出。

[RELY]

官方 Lab 3 已提供且不可改动的接口：`kern/driver/dtb.c` 的 `dtb_init()`（`kern_init` 中已调用）、
`get_memory_base()`、`get_memory_size()`；`pmm.h` 的 `PADDR`/`pa2page`/`page2ppn`（描述符数组从
`DRAM_BASE` 起索引，`page2ppn = page - pages + nbase`）；启动页表仅以
`PHYSICAL_MEMORY_OFFSET = 0xFFFFFFFF40000000` 恒等映射 1GiB 物理内存。QEMU virt 默认内存 128MiB，
`memory` 节点 `reg` 给出 base=0x80000000、size=0x8000000。

[GUARANTEE]

交付可在官方骨架上编译的移植代码；描述符数组紧邻内核镜像（`end` 符号）按页对齐放置；
除描述符区外的整页空闲区间一次性 `init_memmap` 登记；不虚构官方没有的编译目标或测试。

[SPECIFICATION]

### best_fit_pmm.c 的移植

**Pre-Condition**：官方文件已给出 `basic_check`/`best_fit_check` 与文件尾部缺失的
`best_fit_pmm_manager` 定义，占位注释位于第 7 行。

**Post-Condition**：占位处补齐 `best_fit_init/init_memmap/alloc_pages/free_pages/nr_free_pages`
及地址有序链表的插入-合并辅助函数；文件尾补上引用 `best_fit_check` 的管理器结构体定义。

**Requirements**：分配时整表扫描选择 `property >= n` 中块长最小者；恰好匹配整块摘除、余量在
原位拆分为新块；释放按地址插回并与前驱/后继相邻块双向合并；全程维护 `nr_free` 计数守恒。

### pmm.c 的 page_init 移植

**Pre-Condition**：`dtb_init()` 已在 `kern_init` 中完成，`get_memory_base/size` 可用；
`va_pa_offset` 尚未赋值。

**Post-Condition**：`page_init()` 设置 `va_pa_offset`，由 DTB 内存信息推导 `npage`，
初始化全部 `Page` 描述符为 Reserved，将描述符数组之后的整页空闲区间交给 `pmm_manager` 登记。

**Requirements**：`mem_base` 必须等于 `DRAM_BASE`（断言）；描述符数组必须完整落在可用内存内；
不得为虚拟地址绕过 `PADDR`/`va_pa_offset` 约定。

### init_pmm_manager 的接入

**Pre-Condition**：官方默认使用 `&default_pmm_manager`（First-Fit）。

**Post-Condition**：改为 `&best_fit_pmm_manager`，与实验二“Best-Fit 为默认分配器”的行为一致。

## 1. 功能模块一：Trap 上下文恢复与 Trap 入口（trapentry.S）

[PROMPT]

**任务**：在 `kern/trap/trapentry.S` 中完成两处 `//lab3 function1 YOUR CODE` 填空：
(1) 实现与已提供 `SAVE_ALL` 完全对应的 `RESTORE_ALL` 汇编宏；
(2) 补齐 `__alltraps` 中 `SAVE_ALL` 之后的代码，使 trapframe 地址按 RISC-V 调用约定传递给
`void trap(struct trapframe *tf)`，并在 `trap` 返回后无缝落入 `__trapret`。

**操作要求**：`RESTORE_ALL` 的槽位布局必须与 `SAVE_ALL` 逐槽一致（36 × REGBYTES，槽位 0-31 为
通用寄存器，32-35 依次为 sstatus/sepc/sbadaddr/scause）；不得修改 `SAVE_ALL`、`struct trapframe`
与 `trap()` 签名；注释保留 `lab3 function1` 并将 `YOUR CODE` 替换为学号 `2413421`。

**输出要求**：`__alltraps → SAVE_ALL → trap() → __trapret → RESTORE_ALL → sret` 闭环成立；
trap 前后除有意的 `tf->epc` 修改外，所有通用寄存器与 CSR 完整恢复。

[RELY]

`STORE`/`LOAD`/`REGBYTES` 宏取自 `libs/riscv.h`；`SAVE_ALL` 将 trap 前 sp 经 sscratch 中转存入
槽位 2 并把 sscratch 清零；RISC-V 调用约定规定 a0 为第一个整型参数、sp 为被调用者保存寄存器；
`sret` 依据 sepc 与 sstatus.SPP/SPIE 返回被打断的执行流。

[GUARANTEE]

交付的 `RESTORE_ALL` 在 CSR 恢复阶段以 s1-s4 作暂存、且通用寄存器恢复发生在 CSR 写回之后；
sp（槽位 2）最后恢复；不引入对 trapframe 之外内存的读写。

[SPECIFICATION]

### RESTORE_ALL

**Pre-Condition**：sp 指向一个已由 `SAVE_ALL` 填好的完整 trapframe。

**Post-Condition**：sstatus/sepc/sbadaddr/scause 与全部通用寄存器恢复为 trap 前的值，sp 指向
trap 前的栈顶。

**Requirements**：x0 不需要恢复（硬连线为零）；CSR 必须先经通用寄存器中转写回，因此 CSR 恢复
必须先于 s0-s4 所在槽位的覆盖；槽位 2 的 sp 必须最后加载。

### __alltraps 的 C 交接

**Pre-Condition**：`SAVE_ALL` 执行后 sp 指向栈顶 trapframe。

**Post-Condition**：a0 = sp，`trap()` 被调用；返回后 sp 不变并顺序落入 `__trapret`。

**Requirements**：不得在调用 `trap()` 前后使用会破坏 trapframe 的指令序列；依赖调用约定中
sp 的被调用者保存属性，而不是手动重复调整栈指针。

## 2. 功能模块二（之一）：中断处理 interrupt_handler（trap.c）

[PROMPT]

**任务**：在 `kern/trap/trap.c` 的 `//lab3 function2 YOUR CODE` 占位处实现
`void interrupt_handler(struct trapframe *tf)`：时钟中断（IRQ_S_TIMER）调用
`clock_set_next_event()` 重挂下一次事件、`ticks` 计数、每 `TICK_NUM` 次调用 `print_ticks()`
输出统计；统计输出累计达到 `SHUTDOWN_AFTER` 次后调用 `sbi_shutdown()` 关机；其余 S 态中断
输出类型提示，无法识别的中断输出原因并调用 `print_trapframe(tf)` 兜底。

**操作要求**：不改 `trap_dispatch()`、`trap()`、`idt_init()` 等框架函数；注释保留 `lab3 function2`
并将 `YOUR CODE` 替换为学号 `2413456`；`SHUTDOWN_AFTER` 在本文件内自定义。

**输出要求**：时钟中断可持续触发（ticks 无上限增长直到关机条件）；`100 ticks` 每秒输出一次；
官方 `make grade` 的 GRADE_LAB3_TIMER 用例（`[Test 3] ... PASS`、`100 ticks`、`ticks=200`）通过。

[RELY]

`ticks` 为 `clock.c` 中 `volatile size_t`，`clock.h` 提供 `clock_set_next_event()` 声明与
`ticks` 外部引用；`sbi.h` 提供 `sbi_shutdown()`；`riscv.h` 定义 `IRQ_S_TIMER=5`、`IRQ_S_SOFT=1`、
`IRQ_S_EXT=9`，中断的 scause 最高位为 1；`timebase=100000`、QEMU 时钟 10MHz，即每 10ms 一次时钟中断。

[GUARANTEE]

交付的 handler 不修改 trapframe 内除明确说明之外的字段；关机计数只统计 `print_ticks` 的输出次数；
未识别中断先输出 `Unknown interrupt: cause <编号>` 再 `print_trapframe(tf)`。

[SPECIFICATION]

### 时钟中断分支

**Pre-Condition**：`clock_init()` 已设置首个时钟事件且已使能 sie.STIP 与 sstatus.SIE。

**Post-Condition**：每次时钟中断后下一次事件已挂上；`ticks` 严格加一；输出与关机条件满足上述规则。

**Requirements**：必须先重挂事件再处理计数，避免丢失下一次中断；关机前必须完成本轮输出。

### 非 S 态时钟中断的区分

**Pre-Condition**：scause 编码中 IRQ 编号为低位。

**Post-Condition**：软中断输出 `Supervisor software interrupt`，外部中断输出
`Supervisor external interrupt`，其余进入未识别分支。

## 3. 功能模块二（之二）：异常处理 exception_handler（trap.c）

[PROMPT]

**任务**：同一占位处实现 `void exception_handler(struct trapframe *tf)`：对非法指令异常
（CAUSE_ILLEGAL_INSTRUCTION）输出 `Illegal instruction at <epc>`，对断点异常（CAUSE_BREAKPOINT）
输出 `Breakpoint at <epc>`；二者均按触发指令的**真实编码长度**更新 `tf->epc`（读取 epc 处指令
半字的低两位：`0b11` 为 32 位指令 +4，否则为 RVC 压缩指令 +2），使 sret 返回后恰好跳过触发
异常的指令；其余异常直接 `print_trapframe(tf)` 兜底。

**操作要求**：注释与学号要求同上（exception_handler 标注 `2413460`）；不得修改框架函数。

**输出要求**：官方 `make grade` 的 GRADE_LAB3_CONTEXT 用例（断点返回、35 个寄存器上下文完整）
与 GRADE_LAB3_ILLEGAL 用例（非法指令继续执行、`Supervisor software interrupt`、
`Unknown interrupt: cause 15`、`cause    0x00000014` 兜底输出）通过。

[RELY]

RISC-V 指令编码规则：指令半字低两位为 `0b11` 时是 32 位标准指令（如 `.word 0x0000007b` 注入的
非法指令、未压缩的 `ebreak`），否则是 16 位压缩指令——开启 RVC 后汇编器会把 `ebreak` 收缩成
2 字节的 `c.ebreak`；硬件进入异常时 sepc 指向触发异常的指令本身；`print_trapframe`/`print_regs`
为框架提供；grade.sh 以固定子串校验输出。

[GUARANTEE]

仅在两个指定异常分支修改 `tf->epc`，前移量由 `instruction_length()` 读取指令编码得出，
不对任何异常假定固定偏移；默认分支不改 trapframe。

[SPECIFICATION]

### 非法指令与断点分支

**Pre-Condition**：异常发生时 `tf->cause` 为 0x2/0x3，`tf->epc` 指向触发指令，指令本身仍在
内核可读地址空间内。

**Post-Condition**：输出包含 grade.sh 校验子串的一行信息；`tf->epc` 前移真实编码长度；返回后
原执行流跳过异常指令继续。

**Requirements**：不得对所有异常统一 +4——必须读取 epc 处指令半字低两位判长；`ecall` 类异常
实验未要求处理，统一走兜底输出。

## 4. 运行环境适配（非代码功能，问题解决记录）

[PROMPT]

**任务**：官方框架在“Ubuntu 24.04 (WSL) + 系统 QEMU 8.2.2 + riscv64-unknown-elf-gcc 15.1
(binutils 2.45)”下无法构建与运行。请依次定位并解决：汇编器拒绝 `sbadaddr`、`la` 被展开为 GOT
间接寻址、OpenSBI 1.3 (fw_dynamic) 不跳转到 0x80200000、S 模式写 satp 后取指翻车。

**操作要求**：只允许通过“工具链选择、构建参数、QEMU 版本”三类环境手段解决，不得修改官方
`SAVE_ALL`/`entry.S` 的指令序列来迁就新 QEMU；所有环境改动需在报告中记录原因。

**输出要求**：官方 `make` 一次通过；官方 `make qemu` 命令行能直接启动内核。

[RELY]

课程参考环境为 QEMU 4.1.1（自带 OpenSBI 0.4 fw_jump 固定跳转 0x80200000，且旧版 TCG 允许
`csrw satp + sfence.vma` 所在翻译块一次执行完）；Ubuntu 24.04 的 `riscv64-linux-gnu-`
（binutils 2.42）仍保留 priv-1.9 的 `sbadaddr` 汇编助记符；binutils 2.36+ 已移除该名字。

[GUARANTEE]

交付仅为环境层改动：`Makefile` 的 `CFLAGS` 追加 `-fno-pie -fno-pic`（关闭 Ubuntu 交叉 GCC 的
PIE 默认，避免 `la` 走 GOT）；构建使用 `GCCPREFIX=riscv64-linux-gnu-`；本机重建 QEMU 4.1.1；
官方源代码文件内容不被环境适配触碰。

[SPECIFICATION]

### 工具链与构建参数

**Pre-Condition**：系统内同时存在 riscv64-unknown-elf-（binutils 2.45）与 riscv64-linux-gnu-
（binutils 2.42）两套工具链。

**Post-Condition**：以 `make GCCPREFIX=riscv64-linux-gnu-` 构建，CFLAGS 追加 `-fno-pie -fno-pic`。

**Requirements**：解释每个环境问题的因果链（CSR 助记符时代 → binutils 2.36 移除；PIE 默认 →
GOT 寻址；fw_dynamic 仅配合 `-kernel` 才携带 next_addr；satp 写入的翻译生效时机），
不得用与因果无关的偶然性修改掩盖问题。

### QEMU 4.1.1 本地重建

**Pre-Condition**：WSL 无 sudo 权限安装开发依赖。

**Post-Condition**：以 `apt-get download` + `dpkg -x` 将 glib2/pixman/libfdt 开发包解包到用户前缀，
在其中完成 QEMU 4.1.1 的 riscv64-softmmu 构建（`~/qemu-build/qemu-system-riscv64`）。

**Requirements**：源码 SHA-256 校验（ed6fdbbd…a1d2）；构建脚本与产物均不入仓库（已被 .gitignore 覆盖）。
