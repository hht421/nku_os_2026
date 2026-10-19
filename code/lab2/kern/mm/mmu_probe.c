#include <defs.h>
#include <pmm.h>
/* A dedicated initialized page avoids a BSS sweep pre-filling its data TLB entry. */
volatile uintptr_t mmu_probe_value __attribute__((aligned(4096))) = 0x2413460;
uintptr_t __attribute__((noinline)) mmu_read_probe(void) {
    flush_tlb();
    uintptr_t first=mmu_probe_value;
    uintptr_t second=mmu_probe_value;
    assert(first==second);
    return second;
}
