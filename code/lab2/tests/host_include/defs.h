#ifndef LAB2_HOST_DEFS_H
#define LAB2_HOST_DEFS_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define ROUNDUP(a,n) (((a)+(n)-1)/(n)*(n))
#define ROUNDDOWN(a,n) ((a)/(n)*(n))
#define to_struct(ptr,type,member) ((type *)((char *)(ptr)-offsetof(type,member)))
typedef size_t ppn_t;
#endif
