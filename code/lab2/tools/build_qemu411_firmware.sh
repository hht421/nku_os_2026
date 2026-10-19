#!/usr/bin/env bash
# Build the unmodified OpenSBI v0.4 bundled in the official QEMU 4.1.1 archive.
set -euo pipefail
LAB2_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QEMU_ROOT="${1:?usage: bash tools/build_qemu411_firmware.sh QEMU_SOURCE BUILD_DIR}"
BUILD_ROOT="${2:?provide an independent OpenSBI output directory}"
[[ "$(cat "$QEMU_ROOT/VERSION")" == 4.1.1 ]]
mkdir -p "$BUILD_ROOT" "$LAB2_ROOT/firmware"
QEMU_ROOT="$(cd "$QEMU_ROOT" && pwd)"
BUILD_ROOT="$(cd "$BUILD_ROOT" && pwd)"
make -C "$QEMU_ROOT/roms/opensbi" -j"${LAB2_BUILD_JOBS:-4}" \
  CROSS_COMPILE=riscv64-unknown-elf- \
  'CC=riscv64-unknown-elf-gcc -fno-builtin -fno-tree-loop-distribute-patterns' \
  PLATFORM=qemu/virt O="$BUILD_ROOT" FW_JUMP_FDT_ADDR=0x81f00000 \
  FW_DYNAMIC=n FW_PAYLOAD=n
cp "$BUILD_ROOT/platform/qemu/virt/firmware/fw_jump.bin" \
  "$LAB2_ROOT/firmware/opensbi-v0.4-qemu-virt-fdt81f00000.bin"
cp "$QEMU_ROOT/roms/opensbi/COPYING.BSD" "$LAB2_ROOT/firmware/COPYING.BSD"
(cd "$LAB2_ROOT/firmware" && sha256sum opensbi-v0.4-qemu-virt-fdt81f00000.bin > firmware.sha256)
python3 - "$LAB2_ROOT/firmware" <<'PY'
import hashlib, json, sys
from pathlib import Path
directory=Path(sys.argv[1]);meta=directory/'provenance.json'
data=json.loads(meta.read_text()) if meta.exists() else {}
data.update(qemu='4.1.1', opensbi='0.4', modified_third_party_sources=False,
            build_override='FW_JUMP_FDT_ADDR=0x81f00000', kernel_entry='0x80200000',
            source_archive='https://download.qemu.org/qemu-4.1.1.tar.xz',
            source_archive_sha256='ed6fdbbdd272611446ff8036991e9b9f04a2ab2e3ffa9e79f3bab0eb9a95a1d2',
            binary_sha256=hashlib.sha256((directory/'opensbi-v0.4-qemu-virt-fdt81f00000.bin').read_bytes()).hexdigest())
meta.write_text(json.dumps(data,indent=2)+'\n')
PY
