#include <default_pmm.h>
#include <buddy_pmm.h>
#include <dtb.h>
#include <pmm_layout.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <../sync/sync.h>
#include <riscv.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
#if PMM_KIND == 1
    pmm_manager = &default_pmm_manager;
#elif PMM_KIND == 2
    pmm_manager = &buddy_pmm_manager;
#else
    /* lab2 function2 2413460: use Best-Fit in the default kernel and its checks. */
    pmm_manager = &best_fit_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

/* lab2 function1 2413460: derive the layout from DTB, not a fixed RAM size. */
static void page_init(void) {
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    uintptr_t base = get_memory_base(), size = get_memory_size();
    extern char end[];
    struct pmm_layout layout;
    if (base != DRAM_BASE || pmm_compute_layout(base, size, PADDR(end),
          PHYSICAL_MEMORY_END, sizeof(struct Page), &layout) != 0)
        panic("invalid RAM or no room for page descriptors");
    cprintf("physical memory map:\n  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n",
            size, base, base+size-1);
    npage = layout.maxpa / PGSIZE;
    pages = (struct Page *)(layout.descriptors_pa + va_pa_offset);
    for (size_t i = 0; i < layout.descriptors_count; i++) {
        pages[i].flags = 0; SetPageReserved(pages+i);
        pages[i].property = 0; set_page_ref(pages+i, 0);
        list_init(&pages[i].page_link);
    }
    cprintf("managed end: 0x%016lx; descriptors: %lu; free start: 0x%016lx; free pages: %lu\n",
            layout.maxpa, layout.descriptors_count, layout.free_begin, layout.free_pages);
    if (layout.free_pages) init_memmap(pa2page(layout.free_begin), layout.free_pages);
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    if (nr_free_pages()) check_alloc_page();
    else cprintf("No complete free page; allocator checks skipped.\n");

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
