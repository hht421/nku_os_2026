#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>
#define MAX_ORDER 15
#define PG_BUDDY_ALLOC (1UL << 2)
static list_entry_t lists[MAX_ORDER+1];
static size_t free_count;
static unsigned order_for(size_t n) {
    unsigned order=0;
    while (order <= MAX_ORDER && ((size_t)1 << order) < n) order++;
    return order;
}
static void buddy_init(void) {
    for (unsigned o=0; o<=MAX_ORDER; o++) list_init(&lists[o]);
    free_count=0;
}
static void push(struct Page *p, unsigned o) {
    p->property=(size_t)1 << o; SetPageProperty(p);
    list_add(&lists[o], &p->page_link);
}
/* lab2 function3 2413460: PFN-aligned orders, including non-power-of-two arenas. */
static void release_block(struct Page *p, unsigned o) {
    while (o < MAX_ORDER) {
        size_t ppn=page2ppn(p), partner=ppn ^ ((size_t)1 << o);
        if (partner < nbase || partner >= npage) break;
        struct Page *b=pages+(partner-nbase);
        if (!PageProperty(b) || b->property != ((size_t)1 << o)) break;
        list_del(&b->page_link); ClearPageProperty(b); b->property=0;
        if (b<p) p=b;
        o++;
    }
    push(p,o);
}
static void buddy_init_memmap(struct Page *base,size_t n) {
    assert(n>0);
    for (struct Page *p=base;p<base+n;p++) {
        assert(PageReserved(p)); p->flags=p->property=0; set_page_ref(p,0);
        list_init(&p->page_link);
    }
    free_count+=n;
    while (n) {
        unsigned o=0;
        while (o<MAX_ORDER && ((size_t)1 << (o+1))<=n &&
               !(page2ppn(base) & (((size_t)1 << (o+1))-1))) o++;
        release_block(base,o); base+=((size_t)1 << o); n-=((size_t)1 << o);
    }
}
static struct Page *buddy_alloc_pages(size_t n) {
    assert(n>0);
    unsigned want=order_for(n), o=want;
    if (want>MAX_ORDER) return NULL;
    while (o<=MAX_ORDER && list_empty(&lists[o])) o++;
    if (o>MAX_ORDER) return NULL;
    struct Page *p=le2page(list_next(&lists[o]),page_link);
    list_del(&p->page_link); ClearPageProperty(p); p->property=0;
    while (o>want) { o--; push(p+((size_t)1<<o),o); }
    p->flags|=PG_BUDDY_ALLOC; p->property=n;
    free_count-=((size_t)1<<want);
    return p;
}
static void buddy_free_pages(struct Page *base,size_t n) {
    assert(n>0 && (base->flags & PG_BUDDY_ALLOC) && base->property==n);
    unsigned o=order_for(n); assert(o<=MAX_ORDER);
    size_t actual=(size_t)1<<o;
    for (struct Page *p=base;p<base+actual;p++) {
        assert(!PageReserved(p)); p->flags=p->property=0; set_page_ref(p,0);
    }
    free_count+=actual; release_block(base,o);
}
static size_t buddy_nr_free_pages(void) { return free_count; }
static void buddy_check(void) {
    size_t before=nr_free_pages();
    struct Page *a=alloc_pages(3), *b=alloc_pages(5), *c=alloc_page();
    assert(a && b && c && a!=b && a!=c && b!=c);
    assert(!(page2ppn(a)&3) && !(page2ppn(b)&7));
    assert(nr_free_pages()==before-4-8-1);
    free_pages(b,5); free_page(c); free_pages(a,3);
    assert(nr_free_pages()==before);
    assert(alloc_pages(((size_t)1<<MAX_ORDER)+1)==NULL);
    cprintf("buddy_check() succeeded!\n");
}
const struct pmm_manager buddy_pmm_manager={
    .name="buddy_pmm_manager",.init=buddy_init,.init_memmap=buddy_init_memmap,
    .alloc_pages=buddy_alloc_pages,.free_pages=buddy_free_pages,
    .nr_free_pages=buddy_nr_free_pages,.check=buddy_check
};
