#!/usr/bin/env python3
"""Build supplemental hooks in a disposable copy; never alter course tests."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='lab4-extra-') as tmp:
    root = Path(tmp) / 'code'
    shutil.copytree(source, root, ignore=shutil.ignore_patterns(
        'obj', 'bin', '.git', '.qemu.out', '.gdb.in', '*.log', '__pycache__'))
    extra = root / 'kern/extra'
    extra.mkdir()
    for name in ('lab4_extra.c', 'lab4_registers.S', 'lab4_extra.h'):
        shutil.copyfile(source / 'tools' / name, extra / name)
    make = root / 'Makefile'
    make.write_text(make.read_text().replace('# create kernel target',
        '$(call add_files_cc,$(call listf_cc,kern/extra),kernel,$(KCFLAGS) -Ikern/extra)\n\n# create kernel target'))
    for name, anchor, insertion in [
        ('kern/mm/pmm.c', '    kmalloc_init();', '    kmalloc_init();\n    extra_memory();'),
        ('kern/process/proc.c', 'init_main(void *arg)\n{', 'init_main(void *arg)\n{\n    extra_process();')]:
        p = root / name
        text = p.read_text()
        assert text.count(anchor) == 1
        if name.endswith('proc.c'):
            idle = 'void cpu_idle(void)\n{\n    while (1) {'
            assert text.count(idle) == 1
            text = text.replace(idle, idle + '\n        extra_idle();')
        p.write_text('#include "../extra/lab4_extra.h"\n' + text.replace(anchor, insertion))
    subprocess.run(['make', '-j2'], cwd=root, check=True)
    qemu = os.environ.get('QEMU', 'qemu-system-riscv64')
    cmd = [qemu, '-machine', 'virt', '-nographic', '-bios', 'default',
           '-device', 'loader,file=bin/ucore.img,addr=0x80200000']
    print('Supplemental command:', ' '.join(cmd), flush=True)
    run = subprocess.run(cmd, cwd=root, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, timeout=15)
    output = run.stdout.decode(errors='replace')
    print(output, end='')
    markers = ['OOM first/second table allocation rollback PASS',
               'hardware remap/TLB and alias isolation PASS',
               'permissions and Sv39 satp PASS',
               'nested interrupt state restoration PASS',
               'two threads: ra/sp/s0-s11 preserved across yields PASS',
               'all real threads sleeping: idle fallback and wakeup PASS',
               'supplemental checks complete', 'process exit!!']
    assert run.returncode == 0, run.returncode
    assert all(x in output for x in markers) and 'assertion failed' not in output
    print('SUPPLEMENTAL PASS (separate from course grade)')
