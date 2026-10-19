#!/usr/bin/env python3
"""Trace the actual QEMU 4.1.1 MMU implementation with native GDB."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import signal
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'evidence'


def run(name, args, timeout=90):
    process = subprocess.Popen(args, cwd=ROOT, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        output, _ = process.communicate(timeout=timeout)
        code = process.returncode
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        output, _ = process.communicate()
        output += '\nLOCAL_MMU_TIMEOUT\n'
        code = 124
    (OUT / (name + '.log')).write_text(output)
    return subprocess.CompletedProcess(args, code, output)


parser = argparse.ArgumentParser()
parser.add_argument('--qemu', required=True)
parser.add_argument('--source', required=True)
parser.add_argument('--bios', default='firmware/opensbi-v0.4-qemu-virt-fdt81f00000.bin')
parser.add_argument('--output', default=str(OUT))
opt = parser.parse_args()
qemu = Path(opt.qemu).resolve()
source = Path(opt.source).resolve()
OUT = Path(opt.output).resolve()
assert qemu.is_file() and (source / 'target/riscv/cpu_helper.c').is_file()
assert (source / 'VERSION').read_text().strip() == '4.1.1', 'QEMU source must be 4.1.1'
version = subprocess.check_output([str(qemu), '--version'], text=True).splitlines()[0]
assert version == 'QEMU emulator version 4.1.1', version
bios = 'default' if opt.bios == 'default' else str((ROOT / opt.bios).resolve())
if bios != 'default':
    assert Path(bios).is_file()
OUT.mkdir(parents=True, exist_ok=True)
(OUT / 'mmu-results.json').write_text(json.dumps({'passed': False, 'status': 'running'}))
build = run('mmu-kernel-build', ['make', '-j2', 'PMM=best-fit', 'SELFTEST=1'])
assert build.returncode == 0
nm = subprocess.check_output(['riscv64-unknown-elf-nm', '-n', 'bin/kernel'], cwd=ROOT, text=True)
probe = int(re.search(r'^([0-9a-f]+) [A-Za-z] mmu_probe_value$', nm, re.M)[1], 16)
helper = source / 'target/riscv/cpu_helper.c'
lines = helper.read_text().splitlines()
pte_line = next(i + 1 for i, line in enumerate(lines)
                if 'target_ulong ppn = pte >> PTE_PPN_SHIFT' in line)
records = []
for mode, target, access in [('sv39', probe, 0), ('bare', 0x80200000, 2)]:
    expected = target if mode == 'bare' else target - 0xffffffff40000000
    commands = [
        'set pagination off', 'set confirm off',
        'handle SIGPIPE nostop noprint pass', 'handle SIGUSR1 nostop noprint pass',
        f'break get_physical_address if addr == {hex(target)} && access_type == {access}',
        'run', 'echo === NATIVE QEMU 4.1.1 MMU INPUT ===\\n', 'bt 8',
        # 4.1.1 has no is_debug parameter. Verify the actual caller instead.
        'python import gdb; assert gdb.newest_frame().older().name()=="riscv_cpu_tlb_fill"; print("CPU_ACCESS_CALLER_ASSERT_PASS")',
        'p/x addr', 'p/x env->satp', 'p/x env->priv',
        'set $pa_ptr=physical', 'set $expected=' + hex(expected),
    ]
    if mode == 'sv39':
        commands += [
            'up 3', 'echo === SOFTWARE TLB MISS ===\\n',
            'p/x tlb_addr', 'p/x addr', 'p index',
            'python assert (int(gdb.parse_and_eval("tlb_addr"))&~0xfff)!=(int(gdb.parse_and_eval("addr"))&~0xfff); print("TLB_MISS_ASSERT_PASS")',
            'down 3', f'tbreak {helper}:{pte_line} if addr == {hex(target)}',
            'continue', 'echo === PTE READ ===\\n',
            'p levels', 'p ptshift', 'p/x idx', 'p/x pte_addr', 'p/x pte',
            'python assert int(gdb.parse_and_eval("levels"))==3 and int(gdb.parse_and_eval("ptshift"))==18 and int(gdb.parse_and_eval("idx"))==511 and int(gdb.parse_and_eval("pte"))==0x200000cf; print("PTE_ASSERT_PASS")',
        ]
    commands += [
        'finish', 'echo === REAL TRANSLATION RESULT ===\\n',
        'p/x *$pa_ptr', 'p/x $expected',
        'python assert int(gdb.parse_and_eval("*$pa_ptr"))==int(gdb.parse_and_eval("$expected")); print("HOST_MMU_ASSERT_PASS")',
    ]
    if mode == 'sv39':
        commands += [
            f'tbreak tlb_set_page if vaddr == {hex(target)}', 'continue',
            'echo === SOFTWARE TLB FILL ===\\n', 'p/x vaddr', 'p/x paddr', 'p size',
            'python assert int(gdb.parse_and_eval("paddr"))==int(gdb.parse_and_eval("$expected")); print("TLB_FILL_ASSERT_PASS")',
            'finish', 'finish', 'finish',
            'next', 'next', 'p/x entry->addr_read',
            'python assert (int(gdb.parse_and_eval("entry->addr_read"))&~0xfff)==(int(gdb.parse_and_eval("addr"))&~0xfff); print("TLB_CACHE_ASSERT_PASS")',
            'set $probe_translations=1', 'commands 1', 'silent',
            'python assert gdb.newest_frame().older().name()=="riscv_cpu_tlb_fill"',
            'set $probe_translations=$probe_translations+1', 'continue', 'end',
            'continue', 'p $probe_translations',
            'python assert int(gdb.parse_and_eval("$probe_translations"))==1; print("TWO_LOADS_ONE_TRANSLATION_PASS")',
            'quit',
        ]
    else:
        commands += ['kill', 'quit']
    script = OUT / f'mmu-{mode}.gdb'
    script.write_text('\n'.join(commands) + '\n')
    result = run('mmu-' + mode, [
        'gdb', '-q', '-nx', '-batch', '-x', str(script), '--args', str(qemu),
        '-L', str(source / 'pc-bios'), '-machine', 'virt', '-accel', 'tcg,thread=single',
        '-m', '128M', '-nographic', '-bios', bios, '-kernel', 'bin/ucore.img',
    ])
    required = ['CPU_ACCESS_CALLER_ASSERT_PASS', 'HOST_MMU_ASSERT_PASS']
    if mode == 'sv39':
        required += ['TLB_MISS_ASSERT_PASS', 'PTE_ASSERT_PASS', 'TLB_FILL_ASSERT_PASS',
                     'TLB_CACHE_ASSERT_PASS', 'TWO_LOADS_ONE_TRANSLATION_PASS',
                     'LAB2_ALL_TESTS_PASSED']
    passed = result.returncode == 0 and all(word in result.stdout for word in required)
    records.append({'mode': mode, 'target_va': hex(target), 'expected_pa': hex(expected),
                    'passed': passed, 'exit_code': result.returncode})
    print(('PASS ' if passed else 'FAIL ') + mode, flush=True)
summary = {
    'qemu_version': version, 'source_version': '4.1.1',
    'source_archive': 'https://download.qemu.org/qemu-4.1.1.tar.xz',
    'bios': bios,
    'bios_sha256': hashlib.sha256(Path(bios).read_bytes()).hexdigest() if bios != 'default' else None,
    'source_sha256': {name: hashlib.sha256((source / name).read_bytes()).hexdigest()
                      for name in ['target/riscv/cpu_helper.c', 'accel/tcg/cputlb.c']},
    'cpu_access_filter': 'get_physical_address caller must be riscv_cpu_tlb_fill; no guest GDB memory reads',
    'checks': records, 'passed': all(record['passed'] for record in records),
}
(OUT / 'mmu-results.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
raise SystemExit(0 if summary['passed'] else 1)
