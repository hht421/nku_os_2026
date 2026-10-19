#include <pmm_layout.h>
#define PAGE_SIZE 4096UL
#define MAX_VALUE (~(uintptr_t)0)
/* lab2 function1 2413460: checked arithmetic, half-open ranges and page alignment. */
int pmm_compute_layout(uintptr_t base, uintptr_t size, uintptr_t kernel_end,
                       uintptr_t limit, size_t descriptor_size, struct pmm_layout *out) {
    if (!out || !size || !descriptor_size || base % PAGE_SIZE || size > MAX_VALUE - base) return -1;
    uintptr_t end = base + size;
    if (end > limit) end = limit;
    end -= end % PAGE_SIZE;
    if (end <= base || kernel_end < base || kernel_end > end || kernel_end > MAX_VALUE - (PAGE_SIZE-1)) return -1;
    uintptr_t desc = ROUNDUP(kernel_end, PAGE_SIZE);
    size_t count = (end - base) / PAGE_SIZE;
    if (count > (end - desc) / descriptor_size) return -1;
    uintptr_t after = desc + count * descriptor_size;
    /* No complete free page is a valid layout with a zero-length free range. */
    uintptr_t free_begin = end;
    if (after <= MAX_VALUE - (PAGE_SIZE-1)) {
        uintptr_t aligned = ROUNDUP(after, PAGE_SIZE);
        if (aligned <= end) free_begin = aligned;
    }
    *out = (struct pmm_layout){end, desc, free_begin, count, (end-free_begin)/PAGE_SIZE};
    return 0;
}
