#ifndef LAB2_SLUB_H
#define LAB2_SLUB_H
#include <defs.h>
void *slub_alloc(size_t size);
void slub_free(void *ptr);
void slub_check(void);
#endif
