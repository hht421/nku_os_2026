#!/bin/sh
# Bind this checkout explicitly; its temporary container owns all child processes.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
image=${OSLAB_IMAGE:-nku-oslab:ubuntu22.04-arm64}
if [ -n "${OSLAB_QEMU_SOURCE:-}" ]; then
    set -- --mount "type=bind,source=$OSLAB_QEMU_SOURCE,target=/opt/qemu-source,readonly" "$@"
    # Insert the optional mount before the image, not into the guest command.
    mount_option=$2
    shift 2
    exec docker run --rm --init --cap-add SYS_PTRACE --security-opt seccomp=unconfined \
        --mount "type=bind,source=$repo,target=/workspace/nku_os_2026" \
        --mount "$mount_option" -w /workspace/nku_os_2026/code/lab5 "$image" "$@"
fi
exec docker run --rm --init --cap-add SYS_PTRACE --security-opt seccomp=unconfined \
    --mount "type=bind,source=$repo,target=/workspace/nku_os_2026" \
    -w /workspace/nku_os_2026/code/lab5 "$image" "$@"
