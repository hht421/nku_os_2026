#include <console.h>
#include <defs.h>
#include <dtb.h>
#include <kdebug.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <slub.h>
int kern_init(uintptr_t hartid, uintptr_t dtb_pa) __attribute__((noreturn));
int kern_init(uintptr_t hartid, uintptr_t dtb_pa) {
    extern char edata[], end[];
    memset(edata, 0, end-edata);
    extern uintptr_t boot_page_table_sv39[];
    boot_page_table_sv39[2] = 0;
    flush_tlb();
    cons_init();
    dtb_init(hartid, dtb_pa);
    cputs("(THU.CST) os is loading ...");
    print_kerninfo();
    pmm_init();
    extern uintptr_t mmu_read_probe(void);
    cprintf("MMU probe value: 0x%lx\n",mmu_read_probe());
#if LAB2_SELFTEST
    slub_check();
    cprintf("LAB2_ALL_TESTS_PASSED\n");
    sbi_shutdown();
#endif
    while (1) ;
}
