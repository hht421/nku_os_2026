#!/bin/sh
# A fresh container isolates /tmp/lab4_grade and always reaps its QEMU children.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
image=${OSLAB_IMAGE:-nku-oslab:ubuntu22.04-arm64}
exec docker run --rm --init \
    -v "$repo:/workspace/nku_os_2026" \
    -w /workspace/nku_os_2026/code/lab4 "$image" "$@"
