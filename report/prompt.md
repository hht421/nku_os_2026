# Lab 4 提示词任务规格

按课程四段式模板组织；各条完整说明本次实现或分析任务，不把它们标作逐字聊天记录。

## 1．前置内存与中断接口

[PROMPT]
**任务**：补齐当前 Lab 4 框架中 lab2/lab3 标记处的物理内存分配、内存探测和中断恢复代码。
**操作要求**：在 kern/mm/default_pmm.c、pmm.c、kern/trap/trap.c、trapentry.S 内实现；不迁移旧实验整份文件，不改原检查函数和断言。保留原任务注释并填对应成员学号。
**输出要求**：可交叉编译的前置实现，以及本次启动和原框架评分结果。

[RELY]
struct Page 含 ref、flags、property、page_link；free_area_t 含 free_list、nr_free；pmm_manager 提供 init/init_memmap/alloc_pages/free_pages/nr_free_pages/check。
DTB 接口 get_memory_base/get_memory_size；地址偏移 PHYSICAL_MEMORY_OFFSET；pages 按 nbase 对应 DRAM_BASE。trapframe 为 32 个通用寄存器槽和 status/epc/badvaddr/cause 四槽，每槽 REGBYTES=8。

[GUARANTEE]
实现 best-fit 管理器的初始化、分配、释放和空闲页计数，以及 static void page_init(void)。补齐 void interrupt_handler(struct trapframe *tf)、void exception_handler(struct trapframe *tf)、RESTORE_ALL 和 __alltraps 调用 trap 的过程。可增加模块内静态辅助函数。

[SPECIFICATION]
**Pre-Condition**：单核 RISC-V Sv39，启动巨页已建立；DTB 已解析；分配请求 n>0，释放对象属于当前已分配区间。
**Post-Condition**：空闲链表按物理地址排序，分配选择最小可容纳块，释放后合并相邻区间，空闲总数与块长度一致。内存描述符覆盖框架允许的 RAM，内核及描述符占用不进入空闲表。Trap 返回恢复原寄存器、sstatus 和 sepc，最后恢复 sp。
**Case 1**：无合适连续块时返回 NULL，不改变链表和计数。
**Case 2**：时钟中断先重挂下一事件再增加 ticks；断点/非法指令依据指令低两位区分 2/4 字节并前移 epc，其他异常输出现场并 panic。
**Requirements**：保留 basic_check、best_fit_check 和当前框架中断初始化；不引入 Lab 3 自动关机逻辑，不实现用户态栈切换。

## 2．功能模块一：虚拟内存页操作

[PROMPT]
**任务**：完成 kern/mm/pmm.c 的页映射建立、替换与删除，并为启动巨页切换到细粒度页表提供初始化。
**操作要求**：保留原 check_pgdir/check_boot_pgdir；不改测试预期。保留 get_pte 的接口，增加中间表分配失败回滚和高层叶项拒绝；更新 TLB 时约束编译器内存重排。
**输出要求**：可运行的 page_insert/page_remove，原 PMM 评分及 OOM、实际重映射、权限补测。

[RELY]
get_pte(pgdir,la,create)、pte_create、pte2page、page_ref_inc/page_ref_dec、free_page、tlb_invalidate；PTE_V/R/W/X/U/A/D；Sv39 地址索引 PDX1/PDX0/PTX。boot_pgdir_va/pa 分别保存根表的虚拟/物理地址。

[GUARANTEE]
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
允许增加 static void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)，以及启动页表构造所需代码；不把高层巨页当作下一级页表。

[SPECIFICATION]
**Pre-Condition**：pgdir 为有效根表，page 为有效物理页；目标通过普通 4 KiB 叶项维护。启动阶段仍能用临时巨页访问页表页。
**Post-Condition**：成功返回 0，目标叶项指向 page，权限使用传入位，引用计数反映映射数量。页表分配失败返回 -E_NO_MEM，数据页引用与旧映射不变，本次新建但未完成的父表被回收。删除未映射地址不报错。
**Case 1**：相同物理页重复插入只更新权限，引用计数不净增，不释放原页。
**Case 2**：替换不同页时撤销旧映射、减少旧页引用，归零才释放，随后发布新映射。
**Case 3**：删除清空叶项并执行 sfence.vma，引用归零后释放物理页；空中间表可保留。
**Requirements**：建立新根表，代码段 R/X、只读数据段 R、其余直映区 R/W；切换 satp 后刷新 TLB。启用分页后的下一条指令仍须可取指，使用临时恒等映射过渡。

## 3．功能模块二：进程创建及初始化

[PROMPT]
**任务**：实现 kern/process/proc.c 中的 PCB、idleproc 和 initproc 创建链路。
**操作要求**：使用当前 proc_struct、context、trapframe 和链表接口；保留 get_pid、find_proc、init_main 与 do_exit 的框架语义，失败时按资源获得的逆序释放。
**输出要求**：idle PID 0、init PID 1 及可供调度器运行的新线程，原 Process Create 评分结果。

[RELY]
proc_struct 中 state/pid/runs/kstack/need_resched/parent/mm/context/tf/pgdir/flags/name/list_link/hash_link；kmalloc/kfree、setup_kstack/put_kstack、get_pid/hash_proc、wakeup_proc；CLONE_VM、bootstack、boot_pgdir_pa；kernel_thread_entry、forkret、forkrets、__trapret。

[GUARANTEE]
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);

[SPECIFICATION]
**Pre-Condition**：物理页分配、kmalloc 和 VMA 管理已初始化；本实验只创建 current->mm==NULL 的内核线程；调用 do_fork 时 current 和 tf 有效。
**Post-Condition**：新 PCB 初值安全，PID 未分配时为 -1，页表物理地址指向内核根表；成功 fork 返回正 PID，失败返回负错误码，未登记失败对象。idle 使用现有启动栈，init 使用独立两页内核栈。
**Case 1**：线程数已达 MAX_PROCESS 返回 -E_NO_FREE_PROC；PCB/栈分配失败返回 -E_NO_MEM，释放已获得资源。
**Case 2**：copy_mm 保持用户地址空间为空；copy_thread 将 trapframe 放在内核栈顶，子线程 a0=0，context.ra=forkret，context.sp 指向 trapframe。
**Requirements**：kernel_thread 的 s0/s1 保存函数和参数，epc 为 kernel_thread_entry；SPP=1、SPIE=1、SIE=0。PID 分配、哈希/进程链表登记、计数和唤醒在关中断区域完成。

## 4．功能模块三：调度与上下文切换

[PROMPT]
**任务**：补齐 proc.c、kern/schedule/sched.c 和 kern/process/switch.S，让新线程获得 CPU 并可协作让出。
**操作要求**：按现有链表循环选择 runnable 线程；使用 RISC-V Sv39 satp，不调用 Sv32 遗留 lsatp。汇编偏移严格对应 struct context。
**输出要求**：从 idle 切换到 init 的启动结果、原 Schedule 评分，以及多线程寄存器和 idle 保底补测。

[RELY]
current/idleproc/proc_list、PROC_RUNNABLE、need_resched/runs；local_intr_save/local_intr_restore；SATP_MODE_SV39、RISCV_PGSHIFT、flush_tlb；context 按 ra、sp、s0 至 s11 排列，共 14 个 uintptr_t。

[GUARANTEE]
void cpu_idle(void);
void schedule(void);
void proc_run(struct proc_struct *proc);
void switch_to(struct context *from, struct context *to);

[SPECIFICATION]
**Pre-Condition**：进程链表和 idleproc 已初始化，目标线程已有有效根页表和恢复现场；单核且调度发生于协作调用点。
**Post-Condition**：schedule 清除当前重调度标记，选中 runnable 线程并增加其 runs；无合适线程时选 idle。proc_run 在屏蔽中断时更新 current、satp，刷新 TLB，再切换寄存器；旧线程恢复后还原其调用时中断状态。
**Case 1**：idle 的扫描起点为链表头；普通线程的扫描起点为自身后继。表头仅作哨兵，不作为 PCB。
**Case 2**：选中自身时不调用 switch_to；首次进入新线程则沿 forkret/forkrets/__trapret 恢复合成 trapframe 并 sret。
**Requirements**：switch_to 保存并恢复 ra/sp/s0-s11，使用 8 字节槽和 a0/a1 指针。cpu_idle 持续检测 need_resched，不添加时间片抢占。

## 5．扩展练习与 PID 分析

[PROMPT]
**任务**：回答手册两项 Challenge，并结合课堂纪要分析 PID 循环分配。
**操作要求**：以当前 sync.h、get_pte 和 get_pid 为依据，区分 RISC-V 与手册中的 x86 历史表述，不虚构代码改动或运行结论。
**输出要求**：关中断与嵌套恢复说明，Sv32/Sv39/Sv48 对照，get_pte 接口拆分的利弊，以及 PID 防冲突能力与限制。

[RELY]
sync.h 的 __intr_save/__intr_restore 操作 sstatus.SIE；Sv32 为 2 级、每级 10 位 VPN、4 字节 PTE；Sv39/Sv48 分别 3/4 级、每级 9 位 VPN、8 字节 PTE，页内偏移均为 12 位。get_pid 维护 last_pid/next_safe 并遍历 proc_list。

[GUARANTEE]
提供分析正文，不新增分页模式或 PID 代际编码接口；每项结论明确对应当前代码和适用条件。

[SPECIFICATION]
**Pre-Condition**：分析的是本次框架的单核内核线程实现，非用户进程/多核生产系统。
**Post-Condition**：解释 get_pte 两段相似代码来自递归同构的非叶页表，区分页表查找与可分配查找的副作用和失败路径。说明中断恢复只在原 SIE 为 1 时重新使能，嵌套内层不提前开中断。
**Requirements**：PID 分配只保证当前活动集合不重号；退出后复用仍可能让过时引用指向新对象，不能声称解决所有陈旧 PID 问题。

## 6．环境适配与验证

[PROMPT]
**任务**：提供当前 Lab 4 的 Docker 复现入口、原框架评分及独立补充测试。
**操作要求**：使用现有课程镜像、RISC-V 交叉编译器与 QEMU 4.1.1；每次启动隔离临时容器，绑定当前 checkout。原 grade.sh/inject.py/test_lab4.c/test_lab4.h 不改动。日志保存在树外唯一目录，截图读取本次真实日志。
**输出要求**：构建、make grade、make qemu 和补测的命令/版本/退出码；可读图片；测试与补测范围说明。

[RELY]
Makefile 的 grade 在 /tmp/lab4_grade 注入测试；原 do_exit 使用 panic，监视器调用 SBI 关机。当前首次线程 trapframe 的 gp 为 0，编译参数可用 -msmall-data-limit=0 避免小数据 gp 依赖。

[GUARANTEE]
保留 make/grade/qemu 接口；提供 tools/run-in-docker.sh、tools/check_extra.py 及补测源码。可选 GRADE_EVIDENCE 保存评分内核串口输出，失败状态必须传回 make。

[SPECIFICATION]
**Pre-Condition**：Docker 已启动，镜像可用；测试目录是隔离副本，原材料与历史验证日志保持不变。
**Post-Condition**：构建退出 0，原评分正常退出且输出三项 OK 与 Total Score: 30/30，QEMU 真正运行 initproc。补测失败时返回非零，不写入正式内核或替代原评分。
**Requirements**：给 QEMU 设置超时并清理所建进程；记录原始日志与连续截图行范围，注明日志重绘。报告中的 panic 仅在确认为框架 do_exit 预期路径时解释，不普遍豁免断言失败。
