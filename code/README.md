# 实验代码入口

本分支只交付 Lab 4，源码位于 [lab4/Makefile](lab4/Makefile) 所在目录。请用 RISC-V 交叉工具链，勿使用 macOS 默认编译器构建内核。

构建、原框架评分、普通启动及补充测试命令见[仓库 README](../README.md)。`lab4/tools/run-in-docker.sh` 从自身位置解析仓库根目录，每次使用新容器，避免误操作其他实验副本。

`lab4/tools/grade.sh`、`inject.py`、`test_lab4.c`、`test_lab4.h` 与用户提供的原包逐字一致；原有 `basic_check`、`best_fit_check`、页表检查及 VMA 检查保持不变。Makefile 只增加 `-msmall-data-limit=0` 和可选的 `GRADE_EVIDENCE` 日志保存，评分返回值仍向上传递。
