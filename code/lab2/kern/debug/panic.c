#include <assert.h>
#include <defs.h>
#include <intr.h>
#include <sbi.h>
#include <stdio.h>
static bool panicking;
void __panic(const char *file,int line,const char *fmt,...) {
    intr_disable();
    if(!panicking) {
        panicking=1;
        cprintf("kernel panic at %s:%d:\n    ",file,line);
        va_list ap;va_start(ap,fmt);vcprintf(fmt,ap);va_end(ap);cprintf("\n");
    }
    sbi_shutdown();
    while(1);
}
void __warn(const char *file,int line,const char *fmt,...) {
    cprintf("kernel warning at %s:%d:\n    ",file,line);
    va_list ap;va_start(ap,fmt);vcprintf(fmt,ap);va_end(ap);cprintf("\n");
}
bool is_kernel_panic(void) { return panicking; }
