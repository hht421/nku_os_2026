#include <dtb.h>
#include <string.h>
#ifndef LAB2_HOST_TEST
#include <assert.h>
#include <memlayout.h>
#include <stdio.h>
#endif

#define FDT_MAGIC 0xd00dfeedU
#define MAX_DEPTH 32
#define UINTPTR_MAX_VALUE (~(uintptr_t)0)
static uint32_t be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
static int within(size_t off, size_t len, size_t total) {
    return off <= total && len <= total - off;
}
static int terminated(const unsigned char *p, size_t n) {
    for (size_t i = 0; i < n; i++) if (p[i] == 0) return (int)i;
    return -1;
}
static uintptr_t cells(const unsigned char *p, unsigned n) {
    uintptr_t value = 0;
    for (unsigned i = 0; i < n; i++) value = (value << 32) | be32(p + 4 * i);
    return value;
}

/* lab2 function1 2413460: bounded FDT parsing; no heap or fixed RAM size. */
int dtb_parse_memory(const void *blob, size_t limit, uintptr_t *base, uintptr_t *size) {
    if (!blob || !base || !size || limit < 40) return -1;
    const unsigned char *b = blob;
    size_t total = be32(b + 4), so = be32(b + 8), st = be32(b + 12);
    size_t sn = be32(b + 36), tn = be32(b + 32);
    if (be32(b) != FDT_MAGIC || total < 40 || total > limit ||
        so < 40 || st < 40 || !within(so, sn, total) || !within(st, tn, total)) return -1;
    struct node {
        unsigned ac, sc, reg_ac, reg_sc;
        int memory;
        const unsigned char *reg;
        size_t reg_len;
    } stack[MAX_DEPTH];
    unsigned depth = 0;
    size_t pos = 0;
    int found = 0;
    uintptr_t mb = 0, ms = 0;
    while (within(pos, 4, sn)) {
        uint32_t token = be32(b + so + pos); pos += 4;
        if (token == 1) {
            int len = terminated(b + so + pos, sn - pos);
            if (len < 0 || depth == MAX_DEPTH) return -1;
            struct node *n = &stack[depth];
            n->ac = depth ? stack[depth-1].ac : 2;
            n->sc = depth ? stack[depth-1].sc : 1;
            n->reg_ac = n->ac; n->reg_sc = n->sc;
            const char *name = (const char *)(b + so + pos);
            n->memory = !strncmp(name, "memory@", 7) || !strcmp(name, "memory");
            n->reg = NULL; n->reg_len = 0;
            pos = ROUNDUP(pos + (size_t)len + 1, 4);
            if (pos > sn) return -1;
            depth++;
        } else if (token == 3) {
            if (!depth || !within(pos, 8, sn)) return -1;
            size_t len = be32(b + so + pos), no = be32(b + so + pos + 4); pos += 8;
            if (!within(pos, len, sn) || no >= tn || terminated(b + st + no, tn - no) < 0) return -1;
            const char *name = (const char *)(b + st + no);
            const unsigned char *data = b + so + pos;
            struct node *n = &stack[depth-1];
            if (!strcmp(name, "#address-cells") || !strcmp(name, "#size-cells")) {
                if (len != 4) return -1;
                if (!strcmp(name, "#address-cells")) n->ac = be32(data);
                else n->sc = be32(data);
            } else if (!strcmp(name, "device_type")) {
                if (len == 7 && !memcmp(data, "memory", 7)) n->memory = 1;
            } else if (!strcmp(name, "reg")) {
                n->reg = data; n->reg_len = len;
            }
            pos = ROUNDUP(pos + len, 4);
            if (pos > sn) return -1;
        } else if (token == 2) {
            if (!depth) return -1;
            struct node *n = &stack[--depth];
            if (n->memory) {
                if (!n->reg || n->reg_ac < 1 || n->reg_ac > 2 || n->reg_sc < 1 || n->reg_sc > 2) return -1;
                size_t stride = 4 * (n->reg_ac + n->reg_sc);
                if (!n->reg_len || n->reg_len % stride) return -1;
                for (size_t off = 0; off < n->reg_len; off += stride) {
                    uintptr_t a = cells(n->reg + off, n->reg_ac);
                    uintptr_t z = cells(n->reg + off + n->reg_ac * 4, n->reg_sc);
                    if (!z || z > UINTPTR_MAX_VALUE - a) return -1;
                    if (!found) { mb = a; ms = z; found = 1; }
                }
            }
        } else if (token == 4) {
            continue;
        } else if (token == 9) {
            if (depth || !found) return -1;
            *base = mb; *size = ms; return 0;
        } else return -1;
    }
    return -1;
}
#ifndef LAB2_HOST_TEST
static uintptr_t memory_base, memory_size;
void dtb_init(uintptr_t hartid, uintptr_t dtb_pa) {
    /* The bootstrap Sv39 leaf maps QEMU virt's entire 1 GiB DRAM aperture. */
    if (dtb_pa < 0x80000000UL || dtb_pa >= 0xc0000000UL) panic("invalid DTB address");
    size_t limit = 0xc0000000UL - dtb_pa;
    if (limit > 1024 * 1024) limit = 1024 * 1024;
    if (dtb_parse_memory((void *)(dtb_pa + PHYSICAL_MEMORY_OFFSET), limit,
                         &memory_base, &memory_size) != 0) panic("invalid memory information in DTB");
    cprintf("DTB Init\nHartID: %lu DTB Address: 0x%016lx\n", hartid, dtb_pa);
    cprintf("Physical Memory from DTB:\n  Base: 0x%016lx\n  Size: 0x%016lx\n",
            memory_base, memory_size);
    cprintf("DTB init completed\n");
}
uintptr_t get_memory_base(void) { return memory_base; }
uintptr_t get_memory_size(void) { return memory_size; }
#endif
