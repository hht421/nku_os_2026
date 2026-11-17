#!/usr/bin/env python3
"""Observe one real U->S ecall and S->U sret with two live GDBs."""
import os
from pathlib import Path
import re
import select
import signal
import socket
import subprocess
import tempfile
import time


class GDB:
    def __init__(self, label, argv):
        self.label = label
        self.proc = subprocess.Popen(argv + ["-q", "-nx", "-ex", "set pagination off",
            "-ex", "set confirm off", "-ex", "set prompt __OSLAB_GDB__ "],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            start_new_session=True)
        self.buffer = b""
        self.wait()
        self.cmd("set print pretty off")

    def wait(self, timeout=25):
        deadline = time.monotonic() + timeout
        prompt = b"__OSLAB_GDB__ "
        while prompt not in self.buffer:
            if time.monotonic() >= deadline:
                raise RuntimeError(self.label + " GDB timed out: " + self.buffer.decode(errors="replace"))
            if select.select([self.proc.stdout], [], [], 0.1)[0]:
                chunk = os.read(self.proc.stdout.fileno(), 65536)
                if not chunk: raise RuntimeError(self.label + " GDB exited")
                self.buffer += chunk
        out, self.buffer = self.buffer.split(prompt, 1)
        text = out.decode(errors="replace")
        print(text, end="", flush=True)
        return text

    def cmd(self, command, wait=True):
        print(self.label + "> " + command, flush=True)
        self.proc.stdin.write((command + "\n").encode())
        self.proc.stdin.flush()
        return self.wait() if wait else ""

    def value(self, expression):
        out = self.cmd("p/x " + expression)
        match = re.search(r"\$\d+ = (0x[0-9a-f]+|[0-9]+)", out)
        if not match: raise RuntimeError("Cannot read " + expression + ": " + out)
        return int(match[1], 0)

    def interrupt(self):
        os.kill(self.proc.pid, signal.SIGINT)
        return self.wait()

    def close(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try: self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired: self.proc.kill(); self.proc.wait()


def instruction(path, symbol, mnemonic):
    text = subprocess.check_output(["riscv64-unknown-elf-objdump", "-d", str(path)], text=True)
    match = re.search(r"^[0-9a-f]+ <" + symbol + r">:\n(.*?)(?=\n[0-9a-f]+ <|\Z)", text, re.M | re.S)
    if not match: raise RuntimeError("Missing symbol " + symbol)
    insn = re.search(r"^\s*([0-9a-f]+):.*\b" + mnemonic + r"\b", match[1], re.M)
    if not insn: raise RuntimeError("Missing " + mnemonic + " in " + symbol)
    return int(insn[1], 16)


subprocess.run(["make", "-j2"], check=True)
source = Path(os.environ.get("QEMU_SOURCE", "/opt/qemu-source"))
qemu = Path(os.environ.get("QEMU_DEBUG", str(source / "build-lab2/riscv64-softmmu/qemu-system-riscv64")))
if not qemu.is_file():
    raise SystemExit("Debug QEMU missing: set OSLAB_QEMU_SOURCE to the QEMU 4.1.1 source/build directory")
ecall = instruction(Path("obj/__user_exit.out"), "sys_putc", "ecall")
sret = instruction(Path("bin/kernel"), "__trapret", "sret")
with socket.socket() as sock:
    sock.bind(("127.0.0.1", 0)); port = sock.getsockname()[1]
processes = []
with tempfile.TemporaryDirectory(prefix="lab5-gdb-") as tmp:
    serial = Path(tmp) / "qemu.log"
    proc = subprocess.Popen([str(qemu), "-machine", "virt", "-m", "128M", "-nographic",
        "-L", "/usr/local/share/qemu", "-bios", "default", "-monitor", "none",
        "-serial", "file:" + str(serial), "-device", "loader,file=bin/ucore.img,addr=0x80200000",
        "-S", "-gdb", f"tcp:127.0.0.1:{port}"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        guest = GDB("GUEST", ["riscv64-unknown-elf-gdb"]); processes.append(guest)
        guest.cmd("file bin/kernel")
        guest.cmd("add-symbol-file obj/__user_exit.out")
        guest.cmd(f"target remote 127.0.0.1:{port}")
        guest.cmd(f"hbreak *0x{ecall:x}")
        guest.cmd("continue")
        guest.cmd("x/i $pc")
        guest.cmd("info registers pc a0 a1 sp")
        assert guest.value("$pc") == ecall
        assert guest.value("$a0") == 30  # SYS_putc
        guest.cmd("delete breakpoints")
        guest.cmd(f"hbreak __alltraps if $scause == 8 && $sepc == 0x{ecall:x}")

        host = GDB("HOST", ["gdb", str(qemu)]); processes.append(host)
        host.cmd("handle SIGPIPE nostop noprint pass")
        host.cmd("handle SIGUSR1 nostop noprint pass")
        host.cmd(f"set substitute-path /workspace/toolchains/qemu-4.1.1 {source}")
        host.cmd(f"attach {proc.pid}")
        host.cmd("break helper_raise_exception if exception == 8 && env->priv == 0")
        host.cmd("continue", wait=False)
        guest.cmd("continue", wait=False)
        stopped = host.wait()
        assert "helper_raise_exception" in stopped
        host.cmd("bt 5")
        host.cmd("list")
        host.cmd("set $observed_env = env")
        assert host.value("$observed_env->priv") == 0
        assert host.value("$observed_env->pc") == ecall
        host.cmd("tbreak riscv_cpu_do_interrupt")
        host.cmd("continue")
        host.cmd("bt 4")
        host.cmd("finish")
        assert host.value("$observed_env->priv") == 1
        assert host.value("$observed_env->sepc") == ecall
        assert host.value("$observed_env->scause") == 8
        print("ECALL: U -> S; scause=8; sepc=user ecall", flush=True)
        host.cmd("delete breakpoints")
        host.cmd("continue", wait=False)
        guest.wait()
        guest.cmd("x/i $pc")
        guest.cmd("info registers pc sstatus sepc scause stvec")
        guest.cmd("delete breakpoints")
        guest.cmd(f"hbreak *0x{sret:x}")
        guest.cmd("continue")
        guest.cmd("x/i $pc")
        guest.cmd("info registers pc a0 sp sstatus sepc sscratch")
        assert guest.value("$sepc") == ecall + 4
        assert guest.value("$sstatus") & 0x100 == 0
        guest.cmd("delete breakpoints")
        guest.cmd(f"hbreak *0x{ecall + 4:x}")
        host.interrupt()
        host.cmd(f"break helper_sret if env->priv == 1 && env->sepc == 0x{ecall + 4:x}")
        host.cmd("continue", wait=False)
        guest.cmd("continue", wait=False)
        stopped = host.wait()
        assert "helper_sret" in stopped
        host.cmd("bt 5")
        host.cmd("list")
        host.cmd("set $observed_env = env")
        assert host.value("$observed_env->priv") == 1
        host.cmd("finish")
        assert host.value("$observed_env->priv") == 0
        host.cmd("delete breakpoints")
        host.cmd("continue", wait=False)
        guest.wait()
        guest.cmd("x/i $pc")
        guest.cmd("info registers pc a0 sp sstatus sepc")
        assert guest.value("$pc") == ecall + 4
        assert guest.value("$a0") == 0
        print("SRET: S -> U; pc=ecall+4; a0=0", flush=True)
        # The exact translation routines are read from the same checked source tree.
        translated = source / "target/riscv/insn_trans/trans_privileged.inc.c"
        text = translated.read_text()
        assert "trans_ecall" in text and "gen_helper_sret" in text
        print("TCG source:", translated, flush=True)
        print("DUAL GDB PASS", flush=True)
    finally:
        for debugger in reversed(processes): debugger.close()
        if proc.poll() is None:
            proc.terminate()
            try: proc.wait(timeout=3)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait()
        print("--- QEMU serial prefix ---", flush=True)
        if serial.exists(): print(serial.read_text(errors="replace")[:5000], flush=True)
