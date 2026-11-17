#!/usr/bin/env python3
"""Independent boundary checks in a temporary copy; original grade files stay intact."""
from pathlib import Path
import os
import select
import shutil
import signal
import subprocess
import tempfile
import time

kernel_checks = r"""
    kmalloc_trim();
    size_t extra_free = nr_free_pages();
    void *large = kmalloc(8193);
    assert(large != NULL && ksize(large) >= 8193);
    memset(large, 0x5a, 8193);
    kfree(large);
    kmalloc_trim();
    assert(nr_free_pages() == extra_free);
    cprintf("[Extra] kmalloc 8193-byte boundary PASS\n");

    struct Page *root_page = alloc_page();
    assert(root_page != NULL);
    pde_t *root = page2kva(root_page);
    memset(root, 0, PGSIZE);
    list_entry_t held;
    list_init(&held);
    while (nr_free_pages() > 1) {
        struct Page *p = alloc_page();
        assert(p != NULL);
        list_add(&held, &p->page_link);
    }
    assert(get_pte(root, USERBASE, 1) == NULL);
    assert(root[PDX1(USERBASE)] == 0 && nr_free_pages() == 1);
    while (!list_empty(&held)) {
        list_entry_t *le = list_next(&held);
        list_del(le);
        free_page(le2page(le, page_link));
    }
    free_page(root_page);
    assert(nr_free_pages() == extra_free);
    cprintf("[Extra] intermediate page-table OOM rollback PASS\n");

    struct mm_struct *testmm = mm_create();
    assert(testmm != NULL && setup_pgdir(testmm) == 0);
    assert(mm_map(testmm, UTEXT, PGSIZE, VM_READ, NULL) == 0);
    assert(mm_map(testmm, UTEXT - PGSIZE, 3 * PGSIZE, VM_READ, NULL) == -E_INVAL);
    assert(mm_map(testmm, USERTOP - 1, (size_t)-1, VM_READ, NULL) == -E_INVAL);
    assert(!user_mem_check(testmm, UTEXT, 2 * PGSIZE, 0));
    set_mm_count(testmm, 1);
    current->mm = testmm;
    struct proc_struct copy;
    memset(&copy, 0, sizeof(copy));
    assert(copy_mm(CLONE_VM, &copy) == 0 && copy.mm == testmm && mm_count(testmm) == 2);
    mm_count_dec(testmm);
    current->mm = NULL;
    mm_count_dec(testmm);
    exit_mmap(testmm);
    put_pgdir(testmm);
    mm_destroy(testmm);
    kmalloc_trim();
    assert(nr_free_pages() == extra_free);
    cprintf("[Extra] VMA overlap/overflow and CLONE_VM refcount PASS\n");

    struct { struct elfhdr elf; struct proghdr ph; } broken;
    memset(&broken, 0, sizeof(broken));
    broken.elf.e_magic = ELF_MAGIC;
    broken.elf.e_elf[0] = 2; broken.elf.e_elf[1] = 1;
    broken.elf.e_machine = 243;
    broken.elf.e_phentsize = sizeof(struct proghdr);
    broken.elf.e_phoff = sizeof(struct elfhdr);
    broken.elf.e_phnum = 1;
    broken.elf.e_entry = UTEXT;
    broken.ph.p_type = ELF_PT_LOAD;
    broken.ph.p_flags = ELF_PF_R | ELF_PF_X;
    broken.ph.p_va = UTEXT;
    broken.ph.p_memsz = PGSIZE;
    broken.ph.p_filesz = 1;
    broken.ph.p_offset = sizeof(broken) + 1;
    assert(load_icode((unsigned char *)&broken, sizeof(broken)) == -E_INVAL_ELF);
    assert(current->mm == NULL);
    kmalloc_trim();
    assert(nr_free_pages() == extra_free);
    cprintf("[Extra] malformed ELF cleanup PASS\n");
"""
user_checks = r"""
#include <ulib.h>
#include <stdio.h>
#include <error.h>
#include <unistd.h>
static volatile int private_data = 7;
static int raw(uintptr_t number, uintptr_t arg) {
    register uintptr_t a0 asm("a0") = number;
    register uintptr_t a1 asm("a1") = arg;
    register uintptr_t a2 asm("a2") = 0;
    register uintptr_t a3 asm("a3") = 0;
    register uintptr_t a4 asm("a4") = 0;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4) : "memory");
    return (int)a0;
}
int main(void) {
    assert(raw(0, 0) == -E_INVAL && raw(4096, 0) == -E_INVAL);
    assert(raw(SYS_exec, 0) == -E_INVAL);
    assert(kill(1) == -E_INVAL && kill(-1) == -E_INVAL);
    assert(waitpid(-1, NULL) == -E_BAD_PROC);
    int pid = fork(), code;
    assert(pid >= 0);
    if (pid == 0) { private_data = 99; exit(42); }
    assert(waitpid(pid, &code) == 0 && code == 42 && private_data == 7);
    assert(waitpid(pid, NULL) == -E_BAD_PROC);
    cprintf("[Extra user] fork isolation and wait exit-code PASS\n");
    pid = fork();
    assert(pid >= 0);
    if (pid == 0) { *(volatile unsigned int *)(uintptr_t)main = 0; exit(9); }
    assert(waitpid(pid, &code) == 0 && code == -E_KILLED);
    cprintf("[Extra user] read-only text fault contains damage PASS\n");
    pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        int grandchild = fork();
        assert(grandchild >= 0);
        if (grandchild == 0) { for (int i=0; i<10; i++) yield(); exit(23); }
        exit(11);
    }
    assert(waitpid(pid, &code) == 0 && code == 11);
    cprintf("[Extra user] orphan adoption and invalid syscall/exec PASS\n");
    cprintf("edgecases pass.\n");
    return 0;
}
"""

source = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="lab5-extra-") as tmp:
    lab = Path(tmp) / "lab5"
    shutil.copytree(source, lab, ignore=shutil.ignore_patterns("obj", "bin", ".git", "__pycache__"))
    proc = lab / "kern/process/proc.c"
    text = proc.read_text()
    anchor = "static int init_main(void *arg) {"
    assert text.count(anchor) == 1
    proc.write_text(text.replace(anchor, anchor + kernel_checks))
    (lab / "user/edgecases.c").write_text(user_checks)
    subprocess.run(["make", "build-edgecases"], cwd=lab, check=True)
    child = subprocess.Popen(["make", "qemu"], cwd=lab, stdin=subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
    data = bytearray()
    try:
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline:
            if select.select([child.stdout], [], [], 0.1)[0]:
                chunk = os.read(child.stdout.fileno(), 1024)
                if not chunk: break
                data.extend(chunk)
                if b"init check memory pass." in data or b"kernel panic" in data: break
            elif child.poll() is not None: break
    finally:
        if child.poll() is None: os.killpg(child.pid, signal.SIGTERM)
        child.wait(timeout=5)
    output = data.decode(errors="replace")
    print(output, end="")
    required = ["[Extra] kmalloc 8193-byte boundary PASS", "[Extra] intermediate page-table OOM rollback PASS",
        "[Extra] VMA overlap/overflow and CLONE_VM refcount PASS", "[Extra] malformed ELF cleanup PASS",
        "[Extra user] fork isolation and wait exit-code PASS", "[Extra user] read-only text fault contains damage PASS",
        "[Extra user] orphan adoption and invalid syscall/exec PASS", "edgecases pass.", "init check memory pass."]
    if any(s not in output for s in required) or "kernel panic" in output or "user panic" in output:
        raise SystemExit("EXTRA CHECKS FAILED")
    print("\nEXTRA CHECKS PASS (7 independent groups); QEMU group exit:", child.returncode)
