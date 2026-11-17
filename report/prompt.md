# Lab 5 课程模板任务规格

依据当前官方框架与 Lab 5 手册编写；包含本轮用户原始任务及实现前规格。

## 用户原始任务

使用 oslab-delivery，完成 Lab 5，整理成本地已提交、可以直接推送的 lab5 分支。手册使用工作区 lab2026/_book/lab5；框架使用 lab2026/labcode/lab5.zip；本次没有智能纪要。其他材料先从当前工作区查找，缺少的再一次性告知。先不要推送。实际分工和使用的工具参考前面的实验。

## P0 前置继承与双向 Trap 返回

[PROMPT]
**任务**：补齐当前框架 lab2/3/4 标记的物理页管理、页表操作、线程上下文及调度；扩展支持 U/S 双向返回的 RESTORE_ALL。
**操作要求**：kern/mm/default_pmm.c、pmm.c、kern/process/proc.c、switch.S、kern/schedule/sched.c、kern/trap/trapentry.S。保留本版已有 SAVE_ALL、检查函数和断言，按函数迁移同组 Lab4 的必要部分。
**输出要求**：形成对应代码或分析、实际验证结果及报告材料。

[RELY]
Page/free_area/pmm_manager；DTB get_memory_base/get_memory_size；trapframe 含32个GPR及status/epc/tval/cause；sscratch 在用户态保存内核栈顶，在内核态为0。

[GUARANTEE]
page_init、page_insert、page_remove；proc_run、copy_thread、proc_init、schedule、switch_to；RESTORE_ALL 与 __alltraps。

[SPECIFICATION]
**Pre-Condition**：内核使用 RV64/Sv39，启动页表已生效；前序代码来源固定为同组 lab4 提交48d4ca9。
**Post-Condition**：完成物理页发现分配与回收，页映射更新引用计数和TLB；新线程context可恢复；从用户态返回时sscratch保留内核栈顶，恢复原用户sp，内核态返回不污染sscratch。
**Requirements**：同页重映射不增加净引用；分配失败回滚中间页表；用户态和内核态两类Trap都可继续执行。

## F1 用户态系统调用

[PROMPT]
**任务**：实现用户 ecall 到内核分发和返回链路。
**操作要求**：kern/trap/trap.c、kern/syscall/syscall.c 与 user/libs/syscall.c；不改变用户可见系统调用号和现有测试。
**输出要求**：形成对应代码或分析、实际验证结果及报告材料。

[RELY]
libs/unistd.h、current->tf；a0为调用号/返回值，a1到a5为参数；CAUSE_USER_ECALL；do_exit/fork/wait/execve/yield/kill及cputchar。

[GUARANTEE]
exception_handler、syscall和sys_exit/sys_fork/sys_wait/sys_exec/sys_yield/sys_kill/sys_getpid/sys_putc/sys_pgdir；用户库按同一RV64 ABI传递参数。

[SPECIFICATION]
**Pre-Condition**：来自用户的ecall已由SAVE_ALL保存完整tf；非法调用号或用户指针可能由不可信程序传入。
**Post-Condition**：ecall的epc增加4；合法请求分发并将int结果写回a0；无效编号返回-E_INVAL；用户指针在访问前按当前mm验证；用户库不向4字节int执行8字节写入。
**Requirements**：用户异常只结束出错进程；内核异常打印现场并panic；不能把用户错误升级为整个内核崩溃。

## F2 第一个用户进程与 ELF 装载

[PROMPT]
**任务**：实现内核线程转为首个用户进程，以及exec和wait。
**操作要求**：kern/process/proc.c；保留本版set_links/remove_links、PID表和Makefile嵌入用户ELF方式。
**输出要求**：形成对应代码或分析、实际验证结果及报告材料。

[RELY]
proc_struct新增exit_code/wait_state/cptr/yptr/optr；mm_map/setup_pgdir/pgdir_alloc_page；elfhdr/proghdr；USTACKTOP/USTACKSIZE；PTE_U/R/W/X；__trapret。

[GUARANTEE]
alloc_proc、kernel_thread、init_main、user_main、kernel_execve、do_execve、load_icode、do_wait。

[SPECIFICATION]
**Pre-Condition**：用户ELF来自镜像中的binary/size，可能截断或畸形；用户exec参数必须先校验再解除旧mm。
**Post-Condition**：新mm含合法ELF段及用户栈，BSS为零；入口与栈可在U态访问；trapframe清SPP、设SPIE；kernel_execve通过__trapret/sret进入U态；wait检查子进程并回收僵尸的PCB和内核栈；全部用户退出后可检查空闲页恢复；在测量前后调用 kmalloc_trim 归还完全空闲的 SLOB 页，保留相等断言。
**Requirements**：内存不足或ELF非法时释放已构建资源；等待中睡眠与退出唤醒一致；保留内核映射，不给用户页表PTE_U权限访问内核。

## F3 用户进程复制

[PROMPT]
**任务**：实现fork的地址空间复制和子进程上下文。
**操作要求**：kern/process/proc.c、kern/mm/vmm.c、kern/mm/pmm.c；普通fork采用立即复制，COW选做本次不启用。
**输出要求**：形成对应代码或分析、实际验证结果及报告材料。

[RELY]
copy_thread/get_pid/setup_kstack/set_links/hash_proc/wakeup_proc；mm_create/lock_mm/mm_count；VMA区间和PTE权限；CLONE_VM。

[GUARANTEE]
do_fork、copy_mm、dup_mmap、copy_range。

[SPECIFICATION]
**Pre-Condition**：父进程mm有效，或内核线程mm为NULL；输入VMA按地址顺序且不重叠。
**Post-Condition**：父返回子PID、子返回0；子PCB、内核栈、用户物理页独立；VMA范围/权限/数据保持；CLONE_VM增加共享mm计数；失败时不发布半成品进程，释放所有已分配资源。
**Requirements**：不可写文本仍保留只读/执行权限；共享mm只在最后一个持有者退出时销毁；fork产生的子进程挂入正确的兄弟与父子链。

## F4 进程退出与控制

[PROMPT]
**任务**：实现exit、yield、kill与wait配合形成资源回收闭环。
**操作要求**：kern/process/proc.c，沿用本版调度和等待标志；不删除断言或评分检查。
**输出要求**：形成对应代码或分析、实际验证结果及报告材料。

[RELY]
PF_EXITING/WT_CHILD/WT_INTERRUPTED；PROC_SLEEPING/RUNNABLE/ZOMBIE；initproc/idleproc；mm_count/exit_mmap/put_pgdir/mm_destroy。

[GUARANTEE]
do_exit、do_yield、do_kill，配合do_wait；kmalloc_trim仅释放完整空闲页，kmalloc按向上取整页数选择阶数。

[SPECIFICATION]
**Pre-Condition**：当前进程可能拥有或共享mm；目标PID可能不存在、已被kill，或处于可中断等待。
**Post-Condition**：退出先切回内核页表再销毁最后引用的mm，记录退出码并成为僵尸，唤醒等待父进程，子进程转交init；yield置need_resched；kill置PF_EXITING并唤醒可中断等待。
**Requirements**：禁止idle/init退出；重复kill按本框架错误码处理；不能在仍使用自身栈时释放它；退出进程不再返回调用点。

## GDB 系统调用双重 GDB 调试

[PROMPT]
**任务**：完成当前手册指定的guest/host双GDB观察，分别跟踪ecall和sret。
**操作要求**：新增tools/check_gdb.py和复现入口；只读使用QEMU4.1.1源码和带符号可执行文件，不修改第三方源码。
**输出要求**：形成对应代码或分析、实际验证结果及报告材料。

[RELY]
guest GDB加载bin/kernel及obj/__user_exit.out；host GDB可SYS_PTRACE附加专属QEMU PID；QEMU的helper_raise_exception、riscv_cpu_do_interrupt、helper_sret和TCG翻译路径。

[GUARANTEE]
可重复执行的调试脚本；ecall/sret前后PC、特权级及sepc/sstatus/stvec等状态；实际源码调用栈和日志。

[SPECIFICATION]
**Pre-Condition**：用户exit程序构建成功，专用QEMU以-S和gdb端口启动；两个GDB都连接在同一容器内。
**Post-Condition**：实际在用户ecall前停止；host观察U态ecall异常与S态入口；在__trapret的sret前停止并观察返回U态；记录指令与断点证据，结束后只清理本次子进程。
**Requirements**：区分TCG翻译期断点与helper运行期断点；加载用户符号避免只命中内核syscall；断点受TB缓存影响时如实记录。

## A1 分析与报告规格

[PROMPT]
**任务**：回答手册分析题并按教师模板形成报告和任务规格。
**操作要求**：report/report.md、report/prompt.md、report/实验报告模板.md与images；保留教师栏目、实际日志和已核实分工/工具资料。
**输出要求**：形成对应代码或分析、实际验证结果及报告材料。

[RELY]
当前Lab5手册全部页面；Makefile用户ELF嵌入方式；当前源码；真实运行日志、工具版本和用户说明本次无纪要。

[GUARANTEE]
用户镜像预加载与运行时ELF装载的区别；TCG与Lab2地址翻译关系；具体调试问题与AI协作；各模块实现和原理联系；COW选做范围。

[SPECIFICATION]
**Pre-Condition**：分析依据当前实现和已执行证据，不能将历史记录当本次实测。
**Post-Condition**：报告覆盖四模块和双GDB要求，提示词按四段式与代码一致；截图显示真实连续日志及命令/退出码；分工表为沿用责任安排，不编造成员本人复跑。
**Requirements**：记录实际发现的问题与修复；没有出现的问题不编造成多轮迭代；COW未选做则明确范围，无虚构成绩。

