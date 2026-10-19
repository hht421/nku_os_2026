#ifndef LAB2_PMM_LAYOUT_H
#define LAB2_PMM_LAYOUT_H
#include <defs.h>
struct pmm_layout {
    uintptr_t maxpa, descriptors_pa, free_begin;
    size_t descriptors_count, free_pages;
};
int pmm_compute_layout(uintptr_t base, uintptr_t size, uintptr_t kernel_end,
                       uintptr_t limit, size_t descriptor_size, struct pmm_layout *out);
#endif
