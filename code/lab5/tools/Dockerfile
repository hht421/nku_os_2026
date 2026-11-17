FROM ubuntu:22.04 AS base
ARG DEBIAN_FRONTEND=noninteractive
ARG OSLAB_APT_PROXY=""
RUN sed -i 's|http://ports.ubuntu.com/ubuntu-ports|http://mirrors.tuna.tsinghua.edu.cn/ubuntu-ports|g' /etc/apt/sources.list \
    && apt-get -o Acquire::http::Proxy="$OSLAB_APT_PROXY" update && apt-get -o Acquire::http::Proxy="$OSLAB_APT_PROXY" install -y --no-install-recommends \
    build-essential make gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf \
    gdb-multiarch opensbi python3 git ca-certificates bc \
    libglib2.0-0 libpixman-1-0 libfdt1 \
    file less nano vim-tiny wget curl cpio xz-utils patch ripgrep \
    && rm -rf /var/lib/apt/lists/* \
    && ln -s /usr/bin/gdb-multiarch /usr/local/bin/riscv64-unknown-elf-gdb

FROM base AS qemu-build
RUN apt-get -o Acquire::http::Proxy="$OSLAB_APT_PROXY" update \
    && apt-get -o Acquire::http::Proxy="$OSLAB_APT_PROXY" install -y --no-install-recommends \
       pkg-config libglib2.0-dev libpixman-1-dev libfdt-dev \
    && rm -rf /var/lib/apt/lists/*
# Fetch the same official release as the workspace image; verify before building.
RUN wget -O /tmp/qemu-4.1.1.tar.xz https://download.qemu.org/qemu-4.1.1.tar.xz
RUN echo 'ed6fdbbdd272611446ff8036991e9b9f04a2ab2e3ffa9e79f3bab0eb9a95a1d2  /tmp/qemu-4.1.1.tar.xz' | sha256sum -c - \
    && tar -xJf /tmp/qemu-4.1.1.tar.xz -C /opt \
    && mv /opt/qemu-4.1.1 /opt/qemu-source \
    && mkdir /opt/qemu-source/build-lab2 \
    && cd /opt/qemu-source/build-lab2 \
    && /opt/qemu-source/configure --prefix=/usr/local --python=/usr/bin/python3 \
       --target-list=riscv64-softmmu --enable-debug --disable-strip --disable-werror --disable-docs \
       --disable-tools --disable-user --disable-slirp --disable-capstone \
       --disable-gtk --disable-sdl --disable-vnc --enable-fdt \
    && make -j4 \
    && make install DESTDIR=/opt/qemu-install

FROM base
COPY --from=qemu-build /opt/qemu-install/usr/local/ /usr/local/
COPY --from=qemu-build /opt/qemu-source/ /opt/qemu-source/
RUN qemu-system-riscv64 --version | head -n 1 | grep -Fx 'QEMU emulator version 4.1.1'
WORKDIR /workspace/nku_os_2026
CMD ["sleep", "infinity"]
