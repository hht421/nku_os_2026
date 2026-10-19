#include <pmm.h>
#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <buddy_pmm.h>
#include <slub.h>
#include <pmm_layout.h>
#include <dtb.h>
#include <string.h>
#define NPAGES 512
static struct Page descriptors[NPAGES];
struct Page *pages=descriptors;
size_t npage=0x80000+NPAGES;
const size_t nbase=0x80000;
_Alignas(4096) unsigned char host_ram[NPAGES*PGSIZE];
const struct pmm_manager *pmm_manager;
struct Page *alloc_pages(size_t n) { return pmm_manager->alloc_pages(n); }
void free_pages(struct Page *p,size_t n) { pmm_manager->free_pages(p,n); }
size_t nr_free_pages(void) { return pmm_manager->nr_free_pages(); }
static void reset(const struct pmm_manager *m) {
 memset(descriptors,0,sizeof(descriptors));
 for(size_t i=0;i<NPAGES;i++) SetPageReserved(pages+i);
 pmm_manager=m; m->init();
}
static unsigned random_state=0x2413460;
static unsigned rnd(void) { random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state; }
static void selection(void) {
 reset(&best_fit_pmm_manager);
 pmm_manager->init_memmap(pages+1,12);
 pmm_manager->init_memmap(pages+20,5);
 pmm_manager->init_memmap(pages+32,9);
 struct Page *p=alloc_pages(4);assert(p==pages+20);free_pages(p,4);
 p=alloc_pages(9);assert(p==pages+32);free_pages(p,9);
 assert(nr_free_pages()==26);
 reset(&default_pmm_manager);
 pmm_manager->init_memmap(pages+1,12);pmm_manager->init_memmap(pages+20,5);
 p=alloc_pages(4);assert(p==pages+1);free_pages(p,4);
 puts("BEST_FIT_VS_FIRST_FIT_PASS");
}
static void random_pages(int buddy) {
 reset(buddy?&buddy_pmm_manager:&best_fit_pmm_manager);
 size_t initial=buddy?NPAGES-3:NPAGES, start=buddy?3:0;
 pmm_manager->init_memmap(pages+start,initial);
 unsigned char occupied[NPAGES]={0};
 for(size_t i=0;i<start;i++) occupied[i]=1;
 struct entry { struct Page *p;size_t requested,actual; } slots[64]={0};
 size_t used=0;
 for(unsigned iteration=0;iteration<4000;iteration++) {
  unsigned s=rnd()%64;
  if(slots[s].p) {
   size_t at=slots[s].p-pages;
   free_pages(slots[s].p,slots[s].requested);
   for(size_t j=0;j<slots[s].actual;j++) {assert(occupied[at+j]);occupied[at+j]=0;}
   used-=slots[s].actual;slots[s].p=NULL;
  } else {
   size_t n=1+rnd()%19,actual=n;
   if(buddy){actual=1;while(actual<n)actual*=2;}
   size_t expected=NPAGES,smallest=NPAGES+1;
   if(!buddy) {
    for(size_t i=0;i<NPAGES;) {
     if(occupied[i]) {i++;continue;}
     size_t begin=i;while(i<NPAGES&&!occupied[i])i++;
     if(i-begin>=n&&i-begin<smallest){smallest=i-begin;expected=begin;}
    }
   }
   int possible=0;
   if(buddy)for(size_t at=start;at+actual<=NPAGES;at++) {
    if((nbase+at)&(actual-1))continue;
    size_t j=0;while(j<actual&&!occupied[at+j])j++;
    if(j==actual){possible=1;break;}
   }
   struct Page *p=alloc_pages(n);
   if(buddy)assert((p!=NULL)==possible);
   if(!buddy) assert((p?(size_t)(p-pages):NPAGES)==expected);
   if(p) {
    size_t at=p-pages;assert(at>=start&&at+actual<=NPAGES);
    if(buddy)assert(!(page2ppn(p)&(actual-1)));
    for(size_t j=0;j<actual;j++){assert(!occupied[at+j]);occupied[at+j]=1;}
    slots[s]=(struct entry){p,n,actual};used+=actual;
   }
  }
  assert(nr_free_pages()==initial-used);
 }
 for(unsigned i=0;i<64;i++)if(slots[i].p)free_pages(slots[i].p,slots[i].requested);
 assert(nr_free_pages()==initial);
 assert(alloc_pages(NPAGES+1)==NULL);
 if(!buddy){struct Page *p=alloc_pages(NPAGES);assert(p==pages);free_pages(p,NPAGES);}
 puts(buddy?"BUDDY_RANDOM_4000_PASS":"BEST_FIT_RANDOM_4000_PASS");
}
static void original_checks(void) {
 reset(&best_fit_pmm_manager);pmm_manager->init_memmap(pages,NPAGES);pmm_manager->check();assert(nr_free_pages()==NPAGES);
 reset(&default_pmm_manager);pmm_manager->init_memmap(pages,NPAGES);pmm_manager->check();assert(nr_free_pages()==NPAGES);
 reset(&buddy_pmm_manager);pmm_manager->init_memmap(pages,NPAGES);pmm_manager->check();assert(nr_free_pages()==NPAGES);
 puts("PRESERVED_CHECKS_PASS");
}
static void objects(int buddy) {
 reset(buddy?&buddy_pmm_manager:&best_fit_pmm_manager);pmm_manager->init_memmap(pages,NPAGES);
 slub_check();
 void *bulk[300];
 for(unsigned i=0;i<300;i++){bulk[i]=slub_alloc(16);assert(bulk[i]);memset(bulk[i],1+i%254,16);}
 assert(nr_free_pages()==NPAGES-2);
 for(unsigned i=0;i<300;i++)for(unsigned j=0;j<16;j++)assert(((unsigned char *)bulk[i])[j]==1+i%254);
 for(unsigned i=0;i<300;i++)slub_free(bulk[i]);
 assert(nr_free_pages()==NPAGES);
 void *a=slub_alloc(16),*b=slub_alloc(16);assert(a&&b);slub_free(a);
 void *c=slub_alloc(16);assert(c==a);slub_free(b);slub_free(c);
 assert(nr_free_pages()==NPAGES);
 struct object {unsigned char *p;size_t n;unsigned char value;} slots[64]={0};
 for(unsigned k=0;k<2500;k++) {
  unsigned i=rnd()%64;
  if(slots[i].p) {
   for(size_t j=0;j<slots[i].n;j++)assert(slots[i].p[j]==slots[i].value);
   slub_free(slots[i].p);slots[i].p=NULL;
  } else {
   size_t n=1+rnd()%10000;
   unsigned char *p=slub_alloc(n);
   if(p){
    assert(!((uintptr_t)p&15));
    for(unsigned j=0;j<64;j++)if(slots[j].p)
     assert(p+n<=slots[j].p||slots[j].p+slots[j].n<=p);
    unsigned char v=1+rnd()%254;memset(p,v,n);slots[i]=(struct object){p,n,v};
   }
  }
 }
 for(unsigned i=0;i<64;i++)if(slots[i].p)slub_free(slots[i].p);
 assert(nr_free_pages()==NPAGES);
 puts(buddy?"SLUB_BUDDY_RANDOM_2500_PASS":"SLUB_BEST_RANDOM_2500_PASS");
}
static void layout_checks(void) {
 struct pmm_layout x;
 assert(!pmm_compute_layout(0x80000000,0x8000000,0x80200123,0x88000000,40,&x));
 assert(x.descriptors_pa==0x80201000 && x.descriptors_count==32768 && x.free_begin==0x80341000);
 assert(!pmm_compute_layout(0x80000000,0x10000000,0x80200123,0x88000000,40,&x)&&x.maxpa==0x88000000);
 assert(pmm_compute_layout(0x80000000,0,0x80200000,0x88000000,40,&x)<0);
 assert(pmm_compute_layout(0x80000000,~(uintptr_t)0,0x80200000,0x88000000,40,&x)<0);
 assert(pmm_compute_layout(0x80000000,4096,0x80200000,0x88000000,40,&x)<0);
 assert(!pmm_compute_layout(0x80000000,8192,0x80001000,0x88000000,40,&x)&&x.free_pages==0);
 assert(pmm_compute_layout(0x80000000,8192,0x80001fff,0x88000000,40,&x)<0);
 puts("LAYOUT_BOUNDARIES_PASS");
}
static void dtb_checks(const char *directory) {
 const char *names[]={"valid-2cells","valid-1cell","bad-magic","zero-memory","overflow-memory","bad-strings","bad-reg","bad-cells"};
 for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++) {
  char path[512];snprintf(path,sizeof(path),"%s/%s.dtb",directory,names[i]);
  FILE *f=fopen(path,"rb");assert(f);unsigned char b[1024];size_t n=fread(b,1,sizeof(b),f);fclose(f);
  uintptr_t base=0,size=0;int ret=dtb_parse_memory(b,n,&base,&size);
  if(i<2)assert(!ret&&base==0x80000000&&size==0x8000000);else assert(ret<0);
  assert(dtb_parse_memory(b,n-1,&base,&size)<0);
 }
 puts("DTB_VALIDATION_PASS");
}
int main(int argc,char **argv) {
 assert(argc==2 || argc==3);
 if(argc==3){
  reset(&buddy_pmm_manager);pmm_manager->init_memmap(pages,NPAGES);
  if(!strcmp(argv[2],"buddy-size")){struct Page *p=alloc_pages(3);free_pages(p,4);}
  if(!strcmp(argv[2],"buddy-double")){struct Page *p=alloc_pages(3);free_pages(p,3);free_pages(p,3);}
  if(!strcmp(argv[2],"slub-double")){void *p=slub_alloc(32);assert(slub_alloc(32));slub_free(p);slub_free(p);}
  if(!strcmp(argv[2],"slub-interior")){char *p=slub_alloc(32);slub_free(p+1);}
  return 7;
 }
 layout_checks();dtb_checks(argv[1]);selection();original_checks();random_pages(0);random_pages(1);objects(0);objects(1);
 puts("HOST_ALL_TESTS_PASSED");return 0;
}
