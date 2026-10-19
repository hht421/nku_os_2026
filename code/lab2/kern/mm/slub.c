#include <slub.h>
#include <pmm.h>
#include <list.h>
#include <stdio.h>
#include <string.h>
#ifndef LAB2_HOST_TEST
#include <../sync/sync.h>
#else
#define local_intr_save(flag) ((flag)=0)
#define local_intr_restore(flag) ((void)(flag))
#endif
#define MAGIC 0x534c5542U
#define NCLASS 8
static const size_t sizes[NCLASS]={16,32,64,128,256,512,1024,2048};
struct slab {
    uint32_t magic, class_id, capacity, used;
    size_t npages;
    uint64_t bitmap[4];
    list_entry_t link;
    struct Page *page;
};
#define HEADER ROUNDUP(sizeof(struct slab),16)
static list_entry_t caches[NCLASS];
static int initialized;
static void initialize(void) {
    if (!initialized) {
        for(unsigned i=0;i<NCLASS;i++) list_init(&caches[i]);
        initialized=1;
    }
}
/* lab2 optional SLUB Challenge 2413460: page-backed size classes with a per-page allocation bitmap. */
static void *allocate(size_t size) {
    initialize();
    if (!size || size > (~(size_t)0)-HEADER-(PGSIZE-1)) return NULL;
    unsigned cls=0;
    while(cls<NCLASS && sizes[cls]<size) cls++;
    if(cls==NCLASS) {
        size_t n=ROUNDUP(size+HEADER,PGSIZE)/PGSIZE;
        struct Page *p=alloc_pages(n);
        if(!p) return NULL;
        struct slab *s=page2kva(p);
        memset(s,0,HEADER); s->magic=MAGIC; s->class_id=NCLASS;
        s->npages=n; s->page=p;
        return (char *)s+HEADER;
    }
    struct slab *s=NULL;
    list_entry_t *le=&caches[cls];
    while((le=list_next(le))!=&caches[cls]) {
        struct slab *candidate=to_struct(le,struct slab,link);
        if(candidate->used<candidate->capacity) { s=candidate; break; }
    }
    if(!s) {
        struct Page *p=alloc_page();
        if(!p) return NULL;
        s=page2kva(p); memset(s,0,HEADER);
        s->magic=MAGIC; s->class_id=cls; s->page=p; s->npages=1;
        s->capacity=(PGSIZE-HEADER)/sizes[cls];
        assert(s->capacity && s->capacity<=256);
        list_add(&caches[cls],&s->link);
    }
    for(unsigned i=0;i<s->capacity;i++) {
        uint64_t bit=(uint64_t)1<<(i%64);
        if(!(s->bitmap[i/64]&bit)) {
            s->bitmap[i/64]|=bit; s->used++;
            return (char *)s+HEADER+i*sizes[cls];
        }
    }
    panic("SLUB bitmap disagrees with object count");
}
void *slub_alloc(size_t size) {
    bool flag; local_intr_save(flag);
    void *p=allocate(size);
    local_intr_restore(flag); return p;
}
void slub_free(void *ptr) {
    if(!ptr) return;
    bool flag; local_intr_save(flag);
    struct slab *s=(struct slab *)ROUNDDOWN((uintptr_t)ptr,PGSIZE);
    assert(s->magic==MAGIC && s->class_id<=NCLASS);
    if(s->class_id==NCLASS) {
        assert(ptr==(char *)s+HEADER);
        struct Page *p=s->page; size_t n=s->npages;
        s->magic=0; free_pages(p,n);
    } else {
        uintptr_t start=(uintptr_t)s+HEADER;
        size_t slot=sizes[s->class_id];
        assert((uintptr_t)ptr>=start && ((uintptr_t)ptr-start)%slot==0);
        size_t i=((uintptr_t)ptr-start)/slot;
        assert(i<s->capacity && (s->bitmap[i/64]&((uint64_t)1<<(i%64))));
        s->bitmap[i/64]&=~((uint64_t)1<<(i%64)); assert(s->used); s->used--;
        if(!s->used) {
            struct Page *p=s->page; list_del(&s->link); s->magic=0; free_page(p);
        }
    }
    local_intr_restore(flag);
}
void slub_check(void) {
    size_t before=nr_free_pages();
    const size_t requests[]={1,16,17,31,32,33,63,64,65,127,128,129,
                            255,256,257,511,512,513,1023,1024,1025,2048,2049,8193};
    void *objects[sizeof(requests)/sizeof(requests[0])];
    for(unsigned i=0;i<sizeof(requests)/sizeof(requests[0]);i++) {
        objects[i]=slub_alloc(requests[i]); assert(objects[i]);
        assert(!((uintptr_t)objects[i]&15));
        memset(objects[i],(int)(i+1),requests[i]);
    }
    for(unsigned i=0;i<sizeof(requests)/sizeof(requests[0]);i++) {
        unsigned char *p=objects[i];
        for(size_t j=0;j<requests[i];j++) assert(p[j]==i+1);
    }
    for(unsigned i=0;i<sizeof(requests)/sizeof(requests[0]);i++) slub_free(objects[i]);
    assert(slub_alloc(0)==NULL && slub_alloc(~(size_t)0)==NULL);
    slub_free(NULL); assert(nr_free_pages()==before);
    cprintf("slub_check() succeeded!\n");
}
