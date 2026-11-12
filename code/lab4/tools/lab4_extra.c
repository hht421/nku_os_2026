/* Supplemental checks, separate from the unmodified course grading tests. */
#include <assert.h>
#include <error.h>
#include <pmm.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>

static const struct pmm_manager *real_manager;
static int fail_at, allocation_calls;

static struct Page *fail_alloc(size_t n)
{
    if (++allocation_calls == fail_at) return NULL;
    return real_manager->alloc_pages(n);
}

void extra_memory(void)
{
    size_t free_before = nr_free_pages();
    struct Page *root_page = alloc_page(), *target = alloc_page();
    assert(root_page != NULL && target != NULL);
    pde_t *root = page2kva(root_page);
    memset(root, 0, PGSIZE);
    real_manager = pmm_manager;
    struct pmm_manager failing = *real_manager;
    failing.alloc_pages = fail_alloc;
    for (fail_at = 1; fail_at <= 2; fail_at++) {
        allocation_calls = 0;
        size_t before = nr_free_pages();
        pmm_manager = &failing;
        assert(page_insert(root, target, 0, PTE_R | PTE_W) == -E_NO_MEM);
        pmm_manager = real_manager;
        assert(root[0] == 0 && page_ref(target) == 0);
        assert(nr_free_pages() == before);
    }
    free_page(target);
    free_page(root_page);
    assert(nr_free_pages() == free_before);
    cprintf("[Extra] OOM first/second table allocation rollback PASS\n");

    struct Page *p1 = alloc_page(), *p2 = alloc_page();
    assert(p1 != NULL && p2 != NULL);
    volatile uint32_t *va = (volatile uint32_t *)0x4000;
    assert(page_insert(boot_pgdir_va, p1, (uintptr_t)va, PTE_R | PTE_W) == 0);
    *va = 0x11223344;
    assert(page_insert(boot_pgdir_va, p1, (uintptr_t)va + PGSIZE, PTE_R | PTE_W) == 0);
    assert(page_insert(boot_pgdir_va, p2, (uintptr_t)va, PTE_R | PTE_W) == 0);
    *va = 0x55667788;
    assert(*(uint32_t *)page2kva(p1) == 0x11223344);
    assert(*(uint32_t *)page2kva(p2) == 0x55667788);
    page_remove(boot_pgdir_va, (uintptr_t)va);
    page_remove(boot_pgdir_va, (uintptr_t)va + PGSIZE);
    /* Reclaim the test's empty low-address page tables. */
    pde_t *middle = KADDR(PDE_ADDR(boot_pgdir_va[0]));
    free_page(pde2page(middle[0]));
    free_page(pde2page(boot_pgdir_va[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();
    assert(nr_free_pages() == free_before);
    cprintf("[Extra] hardware remap/TLB and alias isolation PASS\n");

    extern char etext[], srodata[], erodata[];
    size_t mapped = 0;
    for (uintptr_t pa = PADDR(KERNBASE); pa < npage * PGSIZE; pa += PGSIZE) {
        uintptr_t va = (uintptr_t)KADDR(pa);
        pte_t *ptep = get_pte(boot_pgdir_va, va, 0);
        assert(ptep != NULL && (*ptep & PTE_V) && PTE_ADDR(*ptep) == pa);
        if (va < ROUNDUP((uintptr_t)etext, PGSIZE)) {
            assert((*ptep & (PTE_R | PTE_X | PTE_W)) == (PTE_R | PTE_X));
        } else if (va >= (uintptr_t)srodata && va < ROUNDUP((uintptr_t)erodata, PGSIZE)) {
            assert((*ptep & (PTE_R | PTE_X | PTE_W)) == PTE_R);
        } else {
            assert((*ptep & (PTE_R | PTE_X | PTE_W)) == (PTE_R | PTE_W));
        }
        mapped++;
    }
    assert((read_csr(satp) >> 60) == SATP_MODE_SV39);
    assert(((read_csr(satp) & ((1ULL << 44) - 1)) << PGSHIFT) == boot_pgdir_pa);
    cprintf("[Extra] direct-map %lu pages, permissions and Sv39 satp PASS\n", mapped);
}

extern int lab4_register_probe(void);
static volatile int finished[2];
static struct proc_struct *waiting;
static volatile int idle_observed;

void extra_idle(void)
{
    if (waiting != NULL) {
        assert(current == idleproc && waiting->state == PROC_SLEEPING);
        idle_observed = 1;
        wakeup_proc(waiting);
        waiting = NULL;
        current->need_resched = 1;
    }
}

static int worker(void *arg)
{
    int id = (intptr_t)arg;
    assert(lab4_register_probe() == 0);
    finished[id] = 1;
    while (1) schedule();
}

void extra_process(void)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    assert(!(read_csr(sstatus) & SSTATUS_SIE));
    bool nested;
    local_intr_save(nested);
    local_intr_restore(nested);
    assert(!(read_csr(sstatus) & SSTATUS_SIE));
    local_intr_restore(intr_flag);
    assert(!!(read_csr(sstatus) & SSTATUS_SIE) == !!intr_flag);
    cprintf("[Extra] nested interrupt state restoration PASS\n");

    struct proc_struct *saved = current;
    int pid0 = kernel_thread(worker, (void *)0, 0);
    int pid1 = kernel_thread(worker, (void *)1, 0);
    assert(pid0 > 1 && pid1 > pid0 && find_proc(pid0)->parent == saved);
    assert(find_proc(pid0)->kstack != find_proc(pid1)->kstack);
    for (int i = 0; i < 16 && !(finished[0] && finished[1]); i++) schedule();
    assert(finished[0] && finished[1] && current == saved);
    cprintf("[Extra] two threads: ra/sp/s0-s11 preserved across yields PASS\n");

    struct proc_struct *p0 = find_proc(pid0), *p1 = find_proc(pid1);
    p0->state = p1->state = saved->state = PROC_SLEEPING;
    waiting = saved;
    schedule();
    assert(idle_observed && current == saved && waiting == NULL);
    p0->state = p1->state = PROC_RUNNABLE;
    cprintf("[Extra] all real threads sleeping: idle fallback and wakeup PASS\n");
    cprintf("[Extra] supplemental checks complete\n");
}
