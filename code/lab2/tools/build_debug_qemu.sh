#!/usr/bin/env bash
set -euo pipefail
# Run in Ubuntu with gcc, make, pkg-config and glib/pixman/libfdt development packages.
LAB2_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QEMU_ROOT="${1:-$LAB2_ROOT/qemu-source}"
BUILD_ROOT="${2:-$LAB2_ROOT/qemu-build}"
if [[ ! -d "$QEMU_ROOT" ]]; then
  ARCHIVE="$(mktemp -t qemu-4.1.1.XXXXXXXX)"
  trap 'rm -f "$ARCHIVE"' EXIT
  if [[ -n "${QEMU_ARCHIVE:-}" && -f "$QEMU_ARCHIVE" ]]; then
    cp "$QEMU_ARCHIVE" "$ARCHIVE"
  else
    curl -fL https://download.qemu.org/qemu-4.1.1.tar.xz -o "$ARCHIVE"
  fi
  printf '%s  %s\n' ed6fdbbdd272611446ff8036991e9b9f04a2ab2e3ffa9e79f3bab0eb9a95a1d2 "$ARCHIVE" | sha256sum -c -
  mkdir -p "$QEMU_ROOT"
  tar -xJf "$ARCHIVE" --strip-components=1 -C "$QEMU_ROOT"
fi
[[ "$(cat "$QEMU_ROOT/VERSION")" == 4.1.1 ]]
QEMU_ROOT="$(cd "$QEMU_ROOT" && pwd)"
mkdir -p "$BUILD_ROOT"
BUILD_ROOT="$(cd "$BUILD_ROOT" && pwd)"
cd "$BUILD_ROOT"
"$QEMU_ROOT/configure" --target-list=riscv64-softmmu --enable-debug --disable-werror \
  --disable-docs --disable-tools --disable-user --disable-slirp --disable-capstone \
  --disable-gtk --disable-sdl --disable-vnc --enable-fdt --python=/usr/bin/python3
make -j"${LAB2_BUILD_JOBS:-4}"
# QEMU 4.x places the executable in the target subdirectory.
ln -sfn riscv64-softmmu/qemu-system-riscv64 "$BUILD_ROOT/qemu-system-riscv64"
