1.我在 WSL Ubuntu 24.04 里搭 riscv64 实验环境，需要能交叉编译内核并用 QEMU 运行。我现在已经按照指南装好了，请你帮我确定一下，检查 riscv64-unknown-elf-gcc、qemu-system-riscv64、make 是否装好、版本是否满足要求(QEMU 不低于 4.1),缺什么就给我安装命令。

2.我需要 make qemu 能把 ucore 内核跑起来，用 QEMU 的 -kernel 参数把 bin/ucore.img 传给固件实现。内核逻辑是 OpenSBI 从 0x80000000 启动后，把内核镜像放到 0x80200000 再跳转过去。当 QEMU 是 8.2、内置 OpenSBI 1.3 时，跳转地址由 -kernel 参数传入；原 Makefile 用 -device loader 只搬数据不设跳转地址，OpenSBI 会跳到 0 地址导致内核不启动。当改成 -kernel 后，OpenSBI 日志的 Domain0 Next Address 应显示 0x80200000。只改 Makefile 的 qemu 和 debug 两个目标，不动内核源码。

3.帮我逐行讲解 kern/init/entry.S,重点回答:la sp, bootstacktop 做了什么、为什么进 C 之前必须做；tail kern_init 和 call 有什么区别、为什么这里必须用 tail。结合 OpenSBI 跳转到 0x80200000 的启动流程讲，最后给我一个能用 GDB 单步验证的思路。

4.这是我们的 GDB 跟踪日志(附原始输出)，帮我按“观察点 → 原始输出 → 推断”整理成调试记录，最后用不超过 200 字回答：加电后最初执行的指令在什么地址、完成了哪些功能。