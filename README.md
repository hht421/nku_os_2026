# Lab 5：用户进程管理

小组仓库为 [hht421/nku_os_2026](https://github.com/hht421/nku_os_2026)，本分支为 `lab5`。代码入口为 `code/lab5/`；正式[报告](report/report.md)、[课程模板提示词](report/prompt.md)和[教师模板](report/实验报告模板.md)位于 `report/`。

## 构建、评分与启动

在当前仓库根目录执行，宿主需已启动 Docker，并有课程镜像 `nku-oslab:ubuntu22.04-arm64`：

```sh
code/lab5/tools/run-in-docker.sh bash -lc 'make clean && make -j2'
code/lab5/tools/run-in-docker.sh make grade
code/lab5/tools/run-in-docker.sh make qemu
```

每条命令使用独立临时容器，明确绑定当前 checkout 到 `/workspace/nku_os_2026`，工作目录为 `/workspace/nku_os_2026/code/lab5`。`obj/`、`bin/` 不进入 Git；`make grade` 清理本实验构建产物，并在该临时容器 `/tmp/lab5_grade` 副本中调用原 `inject.py` 和 `grade.sh`。

默认启动用户程序为 `exit`，完成父子进程创建、等待和退出后输出 `init check memory pass.` 并进入监视器。停止 QEMU 按 Ctrl+A 再按 X。自动验证可运行下面两条命令；脚本只清理自己启动的进程，补充自测只修改临时副本：

```sh
code/lab5/tools/run-in-docker.sh python3 tools/check_boot.py
code/lab5/tools/run-in-docker.sh python3 tools/check_extra.py
```

## 双重 GDB 专项

本实验手册要求观察 QEMU 执行 `ecall/sret`。已有工作区 QEMU 4.1.1 源码和带符号构建时，设置其**宿主绝对路径**：

```sh
OSLAB_QEMU_SOURCE=/absolute/path/to/qemu-4.1.1 \
  code/lab5/tools/run-in-docker.sh python3 tools/check_gdb.py
```

该目录应包含 `target/riscv/` 和 `build-lab2/riscv64-softmmu/qemu-system-riscv64`；只读挂载到 `/opt/qemu-source`。容器使用 `SYS_PTRACE` 与 `seccomp=unconfined`，guest GDB 加载内核及用户 ELF 符号，host GDB 附加同一专属 QEMU 进程。脚本实际检查 `helper_raise_exception`、`riscv_cpu_do_interrupt`、`helper_sret` 的状态以及用户返回地址。`QEMU_SOURCE/QEMU_DEBUG` 可在容器内覆盖默认位置。

没有课程镜像或带符号 QEMU 时，可按仓库配方构建包含源码和调试符号的镜像：

```sh
docker build -f code/lab5/tools/Dockerfile -t nku-oslab:lab5-debug .
OSLAB_IMAGE=nku-oslab:lab5-debug code/lab5/tools/run-in-docker.sh make grade
OSLAB_IMAGE=nku-oslab:lab5-debug code/lab5/tools/run-in-docker.sh python3 tools/check_gdb.py
```

配方从官方地址下载 QEMU 4.1.1 并验证 SHA-256。本次实际验证使用已有课程镜像及工作区带符号 QEMU，没有重新构建该配方。实际版本为 GCC 10.2.0、GDB 12.1、QEMU 4.1.1、默认 OpenSBI v0.4，128 MiB RAM。调试源码与固件由镜像/外部工具链提供，仓库不重新分发大型第三方源码或固件二进制。

## 材料与实现范围

框架为用户指定的官方 `lab5.zip`，内部目录 `lab5_new/`，SHA-256：`4dd863d539fc70eb6df24eaab2b45cda7cbf9c7d47817fbee20bda4926b42ae2`。手册使用 2026-11-17 联网镜像 Lab 5，页面内构建时间 2026-11-10；本次没有智能纪要。同组前置实现来源固定为 `lab4` 提交 `48d4ca9235b3b1745a938eda6a7cfdfb8611f0ac`，按当前框架适配。原评分、注入及测试文件保留原样；`CMakeLists.txt` 是引用缺失旧文件的 IDE 描述，原件保存在树外基线，构建入口为 Makefile。

四个必做功能模块及双重 GDB 专项均纳入验证。普通 fork 立即复制用户页面；COW 为未选做扩展。报告中的终端图片由当前真实日志连续片段重绘，图注标明来源和退出状态；原日志、运行摘要及审写记录保存在本地树外证据目录。本分支停在本地提交，尚未推送。
