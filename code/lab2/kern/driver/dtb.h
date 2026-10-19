#ifndef LAB2_DTB_H
#define LAB2_DTB_H
#include <defs.h>
int dtb_parse_memory(const void *blob, size_t limit, uintptr_t *base, uintptr_t *size);
void dtb_init(uintptr_t hartid, uintptr_t dtb_pa);
uintptr_t get_memory_base(void);
uintptr_t get_memory_size(void);
#endif
