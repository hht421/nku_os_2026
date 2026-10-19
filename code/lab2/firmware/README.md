# QEMU 4.1.1 的 OpenSBI 0.4 启动固件

`opensbi-v0.4-qemu-virt-fdt81f00000.bin` 来自官方 QEMU 4.1.1 发行归档中的 `roms/opensbi/`，未修改第三方源码。保留随附的 BSD-2-Clause 许可证 `COPYING.BSD`。

原 qemu/virt 配置把 DTB 固定搬到 `0x82200000`，超出 32 MiB RAM 的上界 `0x82000000`。本构建仅用 Make 参数覆盖 `FW_JUMP_FDT_ADDR=0x81f00000`，使 DTB 位于本地验证矩阵的最小 32 MiB RAM 内；内核入口仍为 `0x80200000`。它不是新版 OpenSBI，也没有改动内核分配算法或测试断言。

当前配置针对 32 MiB 及以上的本地验证，不承诺低于该容量的固件兼容性。要重现 QEMU 安装时的原默认固件行为，可运行 `make BIOS=default grade`，会重新出现已记录的 32 MiB 启动限制。

在 Linux RISC-V 工具链环境中重建：

```bash
bash tools/build_qemu411_firmware.sh \
  /workspace/toolchains/qemu-4.1.1 \
  /workspace/toolchains/opensbi-v0.4-lab2-safe-build
```

旧裸机代码在 GCC 10 下需要禁用 builtin 和循环转库调用优化，以避免把自身 memset 实现优化成外部 libc 调用；通过编译选项处理，源码保持原样。重建时间会影响固件中的构建横幅及二进制摘要，实际发布副本的 SHA-256 保存在 `firmware.sha256`。QEMU 原始归档 SHA-256 为 `ed6fdbbdd272611446ff8036991e9b9f04a2ab2e3ffa9e79f3bab0eb9a95a1d2`。
