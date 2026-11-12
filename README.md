# Lab 4：进程管理

本分支属于小组仓库 [hht421/nku_os_2026](https://github.com/hht421/nku_os_2026)，实验入口为 `code/lab4/`，正式报告为 [report/report.md](report/report.md)，模板化提示词为 [report/prompt.md](report/prompt.md)。

## 构建与运行

在仓库根目录执行（宿主需启动 Docker）：

```sh
# 已有课程工作区镜像 nku-oslab:ubuntu22.04-arm64 时
code/lab4/tools/run-in-docker.sh make -j2
code/lab4/tools/run-in-docker.sh make grade
code/lab4/tools/run-in-docker.sh make qemu
code/lab4/tools/run-in-docker.sh python3 tools/check_extra.py
```

每条命令都启动独立临时容器，绑定当前仓库到 `/workspace/nku_os_2026`，工作目录为 `code/lab4`；不会运行另一个 checkout 中的内核。源码在宿主上可见，`obj/`、`bin/` 不进入提交。`make grade` 会清理本实验构建产物，并在容器内 `/tmp/lab4_grade` 的副本中注入原测试。

没有课程镜像时，可使用仓库内配方重建：

```sh
docker build -f code/lab4/tools/Dockerfile -t nku-oslab:lab4 .
OSLAB_IMAGE=nku-oslab:lab4 code/lab4/tools/run-in-docker.sh make grade
```

配方从官方地址下载 QEMU 4.1.1，并校验 SHA-256；需要网络、Ubuntu 软件源及构建时间。本次实测使用已有课程镜像，未重新构建该镜像配方。工具版本为 GCC 10.2.0、QEMU 4.1.1、GDB 12.1；QEMU 默认固件输出 OpenSBI v0.4。没有重新分发固件二进制或第三方源码包。

## 结果与范围

本次原框架注入评分为 **30/30**（PMM、Process Create、Schedule 各 10 分）；普通启动完成 VMA 检查并运行 `initproc`。线程函数返回后，框架原有 `do_exit()` 打印 `process exit!!` 并进入监视器，由原有 SBI 关机调用结束 QEMU。它不表示已经实现退出、资源回收或用户进程。

`tools/check_extra.py` 是独立补充验证，检查 OOM 回滚、硬件 TLB 重映射、直映区权限、嵌套中断状态、寄存器保持和 idle 保底。它只改临时副本，不修改原评分文件，也不计入 30 分。

## 材料

当前框架为用户提供的 `lab4.zip`（内部目录 `lab4_new/`），SHA-256 为 `7a92f243f88ca9a4ec78b52e554a14d78a41bbf6a79a1275f6cc929aaebceba2`。课程要求来自新版课程网站镜像的 Lab 4（页面构建时间 2026-11-10，工作区于 2026-11-11 更新），另参考 2026-11-10 课堂智能纪要。原包的 `CMakeLists.txt` 引用了缺失的旧文件，不作为构建入口；原件保存在树外基线。

[教师模板](report/实验报告模板.md)保持原文。报告中的四张图均由本次真实运行日志重绘，标明行范围、命令与退出码；原日志及审阅记录保存在本地树外证据目录。本分支仅本地提交，未推送。
