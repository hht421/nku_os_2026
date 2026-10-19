#ifndef LAB2_HOST_PMM_H
#define LAB2_HOST_PMM_H
#include <defs.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#define PGSIZE 4096UL
#define PGSHIFT 12
#define cprintf printf
#define panic(...) abort()
struct Page { int ref; uint64_t flags; unsigned property; list_entry_t page_link; };
typedef struct { list_entry_t free_list; unsigned nr_free; } free_area_t;
#define SetPageReserved(p) ((p)->flags|=1)
#define PageReserved(p) ((p)->flags&1)
#define SetPageProperty(p) ((p)->flags|=2)
#define ClearPageProperty(p) ((p)->flags&=~2UL)
#define PageProperty(p) ((p)->flags&2)
#define le2page(p,member) to_struct(p,struct Page,member)
struct pmm_manager {
 const char *name; void (*init)(void); void (*init_memmap)(struct Page *,size_t);
 struct Page *(*alloc_pages)(size_t); void (*free_pages)(struct Page *,size_t);
 size_t (*nr_free_pages)(void); void (*check)(void);
};
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern unsigned char host_ram[];
extern const struct pmm_manager *pmm_manager;
static inline size_t page2ppn(struct Page *p) { return (size_t)(p-pages)+nbase; }
static inline uintptr_t page2pa(struct Page *p) { return page2ppn(p)*PGSIZE; }
static inline void *page2kva(struct Page *p) { return host_ram+(p-pages)*PGSIZE; }
static inline int page_ref(struct Page *p) { return p->ref; }
static inline void set_page_ref(struct Page *p,int v) { p->ref=v; }
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *p,size_t n);
size_t nr_free_pages(void);
#define alloc_page() alloc_pages(1)
#define free_page(p) free_pages(p,1)
#endif
