#!/usr/bin/env python3
"""Run make qemu until the complete exit/wait/reclaim chain is observed."""
import os
import select
import signal
import subprocess
import time

proc = subprocess.Popen(["make", "qemu"], stdin=subprocess.DEVNULL,
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
data = bytearray()
deadline = time.monotonic() + 20
try:
    while time.monotonic() < deadline:
        if select.select([proc.stdout], [], [], 0.1)[0]:
            chunk = os.read(proc.stdout.fileno(), 1024)
            if not chunk: break
            data.extend(chunk)
            if b"init check memory pass." in data: break
        elif proc.poll() is not None: break
finally:
    if proc.poll() is None: os.killpg(proc.pid, signal.SIGTERM)
    proc.wait(timeout=5)
text = data.decode(errors="replace")
print(text, end="")
required = ["exit pass.", "all user-mode processes have quit.", "init check memory pass."]
if any(s not in text for s in required) or "kernel panic" in text or "user panic" in text:
    raise SystemExit("BOOT CHECK FAILED")
print("\nBOOT CHECK PASS; make qemu child exit:", proc.returncode)
