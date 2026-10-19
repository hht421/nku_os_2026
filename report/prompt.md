# Lab 2 提示词文档：四段式 Spec 与 Prompt

本文依据新版指导书 Lab 0.5 的[提示词结构](http://oslab.mobisys.top/lab2026/_book/lab0.5/3_prompt_structure.html)整理。对应本地镜像为 `lab/lab2026-html-new/lab2026/_book/lab0.5/3_prompt_structure.html`（2026-10-14 镜像，2026-10-19 核对）。

每条提示词统一使用 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]`：分别说明任务与修改范围、最小可信上下文、必须交付的接口、各接口的行为规格。行为规格包含 Pre-Condition 和 Post-Condition，仅在确有分支或额外约束时加入 Case、Requirements。引用的宏、字段和接口已与当前工程核对；代码摘录去除注释时保留类型与签名。

这些内容是按实验要求改写的可复用提示词，不作为历史对话逐字记录；提示词中的操作要求属于所描述的实验任务，不表示本文改写时重新执行了该任务。Buddy 与选做 SLUB 各保留独立的实现、测试提示词。真实测试结果见 [report.md](report.md)。各任务中的源文件路径相对 `code/lab2/`，带 `report/` 的路径相对仓库根目录，特别注明的 `docker/` 路径相对课程工作区。

## 0. First-Fit 思考题：理解与改进

[PROMPT]

**任务**：阅读 `kern/mm/default_pmm.c`，在仓库根目录 `report/report.md` 第五节写明初始化、登记空闲区、分配、回收合并和可选改进。

**操作要求**：必须修改项目中的真实报告文件；本题只分析现有代码，不修改分配器、测试函数或函数签名。

**输出要求**：依据以下上下文、交付清单与行为规格更新报告，解释采用自己的表述，并给出能区分分配策略的具体例子。

[RELY]

工作目录为 `code/lab2/`，报告路径相对仓库根目录。以下为 `memlayout.h` 中实际字段的摘录，仅去掉注释：

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;
```

`PG_reserved` 表示保留页；`PG_property` 表示空闲块头，`property` 记录块长。链表地址有序，`nr_free` 统计空闲页。需阅读的现有接口为：

```c
static void default_init(void);
static void default_init_memmap(struct Page *base, size_t n);
static struct Page *default_alloc_pages(size_t n);
static void default_free_pages(struct Page *base, size_t n);
```

[GUARANTEE]

交付报告中的 First-Fit 函数解释、一次连续页分配/释放示例和改进分析；不要求新增或修改 C 接口，也不为思考题虚构实现函数。

[SPECIFICATION]

### default_init / default_init_memmap 的报告说明

**Pre-Condition**：报告编写者能够读取上述真实接口及结构，已区分物理页和描述符。

**Post-Condition**：说明初始化后空链表与零计数，以及登记可用区后块头、页状态和计数之间的关系。

**Requirements**：说明 init_memmap 的输入页须为合法保留页；不能把“没有 PG_property”直接解释成“已分配”。
### default_alloc_pages / default_free_pages 的报告说明

**Pre-Condition**：已有地址有序空闲块链表，分配请求为正整数页。

**Post-Condition**：解释首次适配、恰好匹配/切分、失败不改变状态，以及释放后与前后相邻块合并和计数恢复。

**Case 1**：给定 12、5、9 页的空闲块并申请 4 页，First-Fit 选择第一块；报告说明该例与 Best-Fit 的差异。

**Case 2**：释放后出现物理地址相邻的空闲块时，说明双向合并如何恢复连续区间。
### First-Fit 改进分析

**Pre-Condition**：已有对线性扫描和外部碎片的解释。

**Post-Condition**：讨论 next-fit、分桶或树索引的收益与代价，区分查找效率和碎片改善，不给出未经负载验证的绝对优劣结论。

## 1. 物理内存探测与页描述符初始化

[PROMPT]

**任务**：在 `kern/driver/dtb.c` 实现内存信息解析，在 `kern/mm/pmm_layout.c` 实现安全布局计算，在 `kern/mm/pmm.c` 实现 `page_init()`；必要时只调整对应头文件、`kern/init/entry.S` 和 `kern/init/init.c` 的参数交接。

**操作要求**：必须修改项目实际文件并保持公开接口和现有启动逻辑；在 `lab2` 标记处使用学号 `2413460`，不改原始分配器检查。允许新增模块私有 static 辅助函数；纯布局函数是已公开的可测试接口，不把它改为 static。

**输出要求**：依据下列真实声明和行为规格补全模块，说明修改文件与验证方法；不引入堆、进程或多 bank 管理等无关功能。

[RELY]

OpenSBI 入口参数为 a0=hart ID、a1=DTB PA；启动映射允许内核访问交接的 DTB。下列常量和字段摘自当前工程：

```c
#define PGSIZE          4096
#define PGSHIFT         12
#define DRAM_BASE          0x80000000
#define PHYSICAL_MEMORY_END         0x88000000
#define PHYSICAL_MEMORY_OFFSET      0xFFFFFFFF40000000

struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
```

`npage` 为管理上界的绝对 PFN，描述符数量为 `npage-nbase`，`pages[PFN-nbase]` 对应该物理页。可使用 `PADDR`、`pa2page`、`ROUNDUP`、`ROUNDDOWN`、页面状态宏、`panic`、`cprintf`、链接符号 `end`，以及当前管理器的 `init_memmap`；这些依赖不属于本题要重写的接口。

当前启动固件将 DTB 搬至 0x81f00000；本地硬件启动矩阵从 32 MiB 起，纯布局函数的边界测试不依赖能否在更小 RAM 下启动固件。

[GUARANTEE]

必须实现或保持下列接口及布局结构，签名以头文件为准：

```c
int dtb_parse_memory(const void *blob, size_t limit, uintptr_t *base, uintptr_t *size);
void dtb_init(uintptr_t hartid, uintptr_t dtb_pa);
uintptr_t get_memory_base(void);
uintptr_t get_memory_size(void);

struct pmm_layout {
    uintptr_t maxpa, descriptors_pa, free_begin;
    size_t descriptors_count, free_pages;
};
int pmm_compute_layout(uintptr_t base, uintptr_t size, uintptr_t kernel_end,
                       uintptr_t limit, size_t descriptor_size, struct pmm_layout *out);

static void page_init(void);
```

辅助函数仅服务于解析、边界校验或初始化，不改变 PMM 对外接口。

[SPECIFICATION]

### dtb_parse_memory

**Pre-Condition**：blob 非空时指向 limit 字节的可读区域；输出指针非空时指向可写的 uintptr_t。空指针、短输入和坏数据也是待拒绝的输入。

**Post-Condition**：合法输入返回 0 并输出首个内存区的基址和大小；非法输入返回 -1，不输出伪造的默认内存范围。

**Case 1**：FDT 合法时按大端整数读取 1/2 cells 的地址和长度，完成结构解析后才公布结果。

**Case 2**：magic、区域范围、属性、节点层次、字符串终止符、cells、reg 长度或地址加法不合法时拒绝；零长度内存同样拒绝。

**Requirements**：不越过调用者给定的可读边界，不使用尚未初始化的堆；该解析器不是完整的多 bank/reserved-memory 管理器。
### dtb_init

**Pre-Condition**：入口 hartid 和 DTB 物理指针已正确传递，启动映射已建立；候选 DTB 地址必须落在本机支持的映射窗口。

**Post-Condition**：成功时保存解析出的 RAM 信息并打印真实基址和长度，供 getter 使用；非法地址或信息导致 panic。

**Requirements**：指针转换遵循当前映射；不覆盖尚未解析的交接参数，不将错误静默替换成固定 128 MiB。
### get_memory_base

**Pre-Condition**：dtb_init 已成功完成。

**Post-Condition**：返回先前保存的 RAM 物理基址，不修改解析结果或重新分配资源。
### get_memory_size

**Pre-Condition**：dtb_init 已成功完成。

**Post-Condition**：返回先前保存的 RAM 长度，不修改解析结果或重新分配资源。
### pmm_compute_layout

**Pre-Condition**：输入值按 uintptr_t/size_t 表示；out 非空时指向可写结构，NULL 输出和非法参数须被拒绝。

**Post-Condition**：成功返回 0，输出互不侵占的描述符区、管理上界与完整空闲页范围；失败返回 -1，不发布部分布局。

**Case 1**：内存范围与描述符容量合法时，管理上界受 limit 限制并向下页对齐，描述符起点和空闲起点向上页对齐。

**Case 2**：信息无效、地址/长度运算溢出、内核越界或描述符放不下时失败，不产生可注册区间。

**Case 3**：描述符放得下但没有完整空闲页时仍返回合法布局，free_pages 为 0。

**Requirements**：纯计算接口，不访问真实 RAM、不修改分配器；先检查算术和空间，再进行对齐及布局发布。
### page_init

**Pre-Condition**：内核处于高地址映射，DTB 已初始化，所选 pmm_manager 已完成 init；链接符号和描述符所在内存可写。

**Post-Condition**：设置 va_pa_offset、npage、pages；固件、内核和描述符占用页不进入空闲区，剩余完整页交给所选管理器。

**Case 1**：RAM 基址及布局合法且有空闲页时，先初始化并保留全部受管描述符，再注册可分配区。

**Case 2**：基址不适配当前启动映射，或 RAM/布局无效时 panic，不继续分配。

**Case 3**：没有完整空闲页时完成合法元数据初始化，但不调用零长度 init_memmap。

**Requirements**：区分 VA、PA、PFN 和 Page 指针；保留框架管理上界，256 MiB 探测结果不能被伪写成 128 MiB。

## 2. Best-Fit 连续物理内存分配

[PROMPT]

**任务**：补全 `kern/mm/best_fit_pmm.c` 的管理函数与注册对象，并在 `kern/mm/pmm.c` 的 `init_pmm_manager()` 中将默认管理器接为 Best-Fit；对应声明位于 `best_fit_pmm.h`。

**操作要求**：必须修改实际文件，保留原始 basic_check、best_fit_check 的全部文本与断言；不重写 First-Fit。新实现保留 lab2/2413460 标记，模块状态私有，允许新增 static 合并辅助函数。

**输出要求**：按照下列接口和规格实现模块，给出默认接入点、选择策略与改进空间；使用既有检查和独立选择反例验证。

[RELY]

以下结构和声明取自当前 `pmm.h`、`memlayout.h`，仅去注释：

```c
struct pmm_manager {
    const char *name;
    void (*init)(
        void);
    void (*init_memmap)(
        struct Page *base,
        size_t n);
    struct Page *(*alloc_pages)(
        size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
```

```c
#define PG_reserved                 0
#define PG_property                 1
#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)
```

可使用 `list.h` 的双向链表操作和 `set_page_ref`。PMM 包装层负责单核中断保护；管理函数不更改公开 ABI。

`default_pmm.c` 已定义自己的 free_area，不可引入同名外部定义。原始检查依赖块头 property、空闲链表和计数约定；初始可用区由 page_init 注册。

[GUARANTEE]

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
static void init_pmm_manager(void);
extern const struct pmm_manager best_fit_pmm_manager;
```

注册对象必须填齐 pmm_manager 的六个回调；check 指向保留的 best_fit_check。

[SPECIFICATION]

### best_fit_init

**Pre-Condition**：没有尚待保留的本模块分配状态，允许初始化私有管理区。

**Post-Condition**：空闲链表为空、空闲页计数为 0，各状态属于 Best-Fit 模块自身。
### best_fit_init_memmap

**Pre-Condition**：管理器已初始化，n>0，base 起的 n 个描述符是未登记且合法的保留页，范围不与已有可用区重叠。

**Post-Condition**：这些页成为可分配资源，块头、链表顺序与空闲计数一致，原有可用页不丢失。

**Requirements**：只将空闲块头标为 PG_property，清除可用页的保留状态并维护连续空闲区。
### best_fit_alloc_pages

**Pre-Condition**：请求 n>0，空闲链表和计数满足不变量。

**Post-Condition**：成功返回最小足够空闲块的起始页且恰好交出 n 页；失败返回 NULL，状态保持不变。

**Case 1**：存在恰好匹配块时移出该空闲块，正确清除分配块头的空闲标记。

**Case 2**：最小足够块更大时，仅交出请求部分，余量仍可分配且链表地址顺序正确。

**Case 3**：总空闲页不足或没有足够连续块时返回 NULL；相同大小候选选择低地址块。

**Requirements**：空闲计数等于全部空闲块长度之和；不能把 First-Fit 的首个匹配块当作 Best-Fit 结果。
### best_fit_free_pages

**Pre-Condition**：n>0，base 起的页面均属于可合法回收的已分配区间，未保留且未与现有空闲块重叠。

**Post-Condition**：回收区间重新可用，空闲计数增加 n，地址相邻的前后空闲块合并，链表与块头状态一致。

**Case 1**：与前后均不相邻时形成独立空闲块。

**Case 2**：与一侧或两侧相邻时合并对应区间，失去块头身份的描述符不再带有效空闲块头标记。

**Requirements**：参数错误可通过原有断言拒绝，不削弱既有检查来接受非法回收。
### best_fit_nr_free_pages

**Pre-Condition**：模块状态有效。

**Post-Condition**：返回当前可分配物理页总数，不改变状态。
### init_pmm_manager / best_fit_pmm_manager

**Pre-Condition**：构建参数可选择 best-fit、first-fit 或 buddy，对应注册对象存在。

**Post-Condition**：默认配置实际使用 Best-Fit 并执行其初始化和原始检查；显式选择其他分配器仍有效。

**Requirements**：保留已有可切换入口；报告讨论线性扫描、分桶/树索引与碎片的权衡。

## 3. Buddy System：实现提示词

[PROMPT]

**任务**：从零组织 `kern/mm/buddy_pmm.c`、`buddy_pmm.h` 的 Buddy 管理模块，接入 `pmm.c` 与 Makefile 的 PMM=buddy 选择；已存在的实现按本规格补全，不用独立用户程序替代内核接入。

**操作要求**：必须修改实际工程文件，保留默认 Best-Fit 和原始 First/Best-Fit 检查；只修改 Buddy 及其接入位置。私有辅助函数和状态使用 static；不改变 Page 布局和 PMM 公开签名。

**输出要求**：依据四段式内容交付可编译运行的管理器和基本自检；完整独立测试由下一条提示词单独完成。

[RELY]

以下结构和声明取自当前 `pmm.h`、`memlayout.h`，仅去注释：

```c
struct pmm_manager {
    const char *name;
    void (*init)(
        void);
    void (*init_memmap)(
        struct Page *base,
        size_t n);
    struct Page *(*alloc_pages)(
        size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
```

```c
#define PG_reserved                 0
#define PG_property                 1
#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)
```

可使用 `list.h` 的双向链表操作和 `set_page_ref`。PMM 包装层负责单核中断保护；管理函数不更改公开 ABI。

`page2ppn` 返回物理 PFN，而非数组下标；pages 对应 [nbase,npage)。当前 Buddy 模块约定 MAX_ORDER=15，请求与回收通过现有 alloc_pages/free_pages 包装层进入。

[GUARANTEE]

```c
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);
extern const struct pmm_manager buddy_pmm_manager;
```

注册对象应填齐所有回调；接入处复用既有 init_pmm_manager(void)，使显式 PMM=buddy 选中 Buddy。

[SPECIFICATION]

### buddy_init

**Pre-Condition**：无须保留旧 Buddy 分配状态。

**Post-Condition**：各阶自由链表有效且为空，空闲页计数为 0。
### buddy_init_memmap

**Pre-Condition**：管理器已初始化，n>0，输入为尚未登记的合法保留页区间；不同输入区间不重叠。

**Post-Condition**：只登记输入可用页；每个空闲块大小为 2 的幂、物理 PFN 按块大小对齐，所有空闲块覆盖范围与输入一致。

**Case 1**：区间起点未按大块对齐或长度非 2 的幂时仍能完整登记，不多交出区间外的页面。

**Case 2**：相邻可用区能够构成合法同阶伙伴时允许合并；保留页和越界页不能被合并进来。
### buddy_alloc_pages

**Pre-Condition**：请求 n>0，管理区状态有效。

**Post-Condition**：成功返回包含至少 n 页的最小支持阶块，实际占用为请求向上取整后的幂次页数，头部保留原请求长度；失败时状态不变。

**Case 1**：有合适空闲块时交出对齐、不重叠的区间，必要时让未交出部分继续可分配。

**Case 2**：请求超过最大阶或没有可满足的块时返回 NULL。

**Requirements**：3 页请求实际扣 4 页、5 页扣 8 页；空闲计数按实际块大小变化。
### buddy_free_pages

**Pre-Condition**：base 为本模块尚未释放的分配头，n>0，且 n 必须与原请求一致。

**Post-Condition**：整块回收后页计数恢复，能与真正同阶空闲伙伴递归合并，不能越界或并入保留页。

**Case 1**：回收参数合法时恢复可用空间；伙伴是否可合并由物理 PFN、阶和空闲状态共同决定。

**Case 2**：释放长度错误、重复释放或非分配头输入不属于合法接口调用，须由断言拒绝而不是破坏链表。

**Requirements**：不支持将已分配块任意拆开部分释放；正常包装层的单核序列化约束保持不变。
### buddy_nr_free_pages

**Pre-Condition**：Buddy 状态有效。

**Post-Condition**：返回实际可用页数，不把向上舍入造成的内部碎片算作仍可用页面。
### buddy_check

**Pre-Condition**：内核可用区能满足受控的基本分配请求。

**Post-Condition**：通过断言检验 3/5 页舍入、PFN 对齐、释放后计数恢复和超阶失败；检查完成不遗留分配。
### Buddy 注册与选择

**Pre-Condition**：上述接口存在，构建参数为合法 PMM 值。

**Post-Condition**：PMM=buddy 实际运行 Buddy，默认配置仍选择 Best-Fit。

## 4. Buddy System：独立测试提示词

[PROMPT]

**任务**：在 `tests/test_allocators.c` 增补 Buddy 独立模型与边界测试，完善 `kern/mm/buddy_pmm.c` 的 buddy_check，并在 `tools/test_lab2.py` 接入宿主负例和真实内核启动验证。

**操作要求**：必须修改实际测试文件；不得改动 Buddy 分配策略迎合测试，不修改原始 First/Best-Fit 检查或删除失败用例。允许新增 static 测试辅助函数，保留既有测试入口。

**输出要求**：交付下列测试函数和现有脚本入口，说明覆盖的行为、边界与判定依据；记录实际退出码和原始输出。

[RELY]

现有宿主适配层为 `tests/host_include/pmm.h`，有真实的 Page/PMM 声明和 host_ram 地址转换。测试中的固定环境摘录如下：

```c
#define NPAGES 512
static struct Page descriptors[NPAGES];
struct Page *pages=descriptors;
size_t npage=0x80000+NPAGES;
const size_t nbase=0x80000;
_Alignas(4096) unsigned char host_ram[NPAGES*PGSIZE];
```

`reset(const struct pmm_manager *m)` 初始化受控页区，alloc_pages/free_pages/nr_free_pages 调用被测管理器；测试模型必须独立于其空闲链表。宿主断言由 libc abort，内核断言使用现有 panic。

Buddy 已实现上一条规定的 PMM 接口，MAX_ORDER=15，只能按原请求整体释放。验收脚本使用 AddressSanitizer/UndefinedBehaviorSanitizer，负例运行在单独宿主进程。

[GUARANTEE]

```c
static void random_pages(int buddy);
static void original_checks(void);
int main(int argc, char **argv);
static void buddy_check(void);
```

`random_pages(1)` 覆盖 Buddy；保留 `random_pages(0)` 对 Best-Fit 的既有覆盖。脚本使用现有 `host_tests()`、`qemu_tests()`、`main()` 和 `make grade` 接入，不另建不被执行的测试文件。

[SPECIFICATION]

### random_pages(1)

**Pre-Condition**：使用新的受控 Buddy 页区，初始化前没有存活分配。

**Post-Condition**：4000 次确定种子的申请/释放后，独立占用模型与实际返回区间、PFN 对齐和页计数一致，全部释放后恢复初始资源。

**Case 1**：成功分配时检查范围合法、无重叠、按实际幂次大小扣账。

**Case 2**：分配失败时，模型也必须找不到合法对齐连续块，避免错误返回 NULL 被误判通过。

**Case 3**：采用从下标 3 开始的 509 页区间，前缀保留页不能被交出；同时覆盖超容量/超阶失败。

**Requirements**：模型不读取被测自由链表，也不复用 Buddy 的内部选块结论。
### original_checks / buddy_check

**Pre-Condition**：分配器已在可满足基本用例的独立测试区初始化。

**Post-Condition**：原始 First/Best-Fit 检查仍执行，Buddy 检查验证舍入、拆分/合并与回收，检查后无资源泄漏。
### main 的 Buddy 负例入口

**Pre-Condition**：argc 为已有测试协议接受的 2 或 3，额外参数可指定 buddy-size 或 buddy-double。

**Post-Condition**：正常入口运行完整断言后成功退出；错误长度和重复释放由独立子进程按预期 SIGABRT 结束。

**Requirements**：不能把任意非零退出都当作负例通过；无法执行、超时或其他崩溃应区分记录。
### host_tests / qemu_tests / main 的验收接入

**Pre-Condition**：Linux 工具链和 QEMU 可用，证据输出位置已检查并备份。

**Post-Condition**：宿主 sanitizer、负例和 Buddy 在 32/64/128/256 MiB 下的真实启动均实际执行，汇总反映各项真实结果。

**Requirements**：有限测试只说明覆盖用例通过；未取得官方评分包时不得称为官方 grade。

## 5. 选做 SLUB：实现提示词

[PROMPT]

**任务**：在 `kern/mm/slub.c`、`slub.h` 中从零实现简化对象分配模块，并在 `kern/init/init.c` 的既有自检流程接入；已经存在的代码按下列规格补全。

**操作要求**：必须修改实际文件；不引入现成 kmalloc 或完整 Linux SLUB，不更改 PMM/Page ABI。模块私有状态和辅助函数保持 static，保留既有页分配器检查。

**输出要求**：使用下面的真实页接口交付两层分配器及基本自检，解释支持的大小、对齐、释放约束与单核适用范围。

[RELY]

下层由 pmm.h 提供如下接口：

```c
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
```

可使用 `page2kva`、`list.h`、memset、ROUNDUP/ROUNDDOWN 和单核中断保护宏。页大小由工程 PGSIZE 定义。Best-Fit 精确分配页数，Buddy 可能向上舍入，回收必须传回原请求。上层尺寸类为 16 至 2048 字节的八个幂次大小，返回地址按 16 字节对齐。

[GUARANTEE]

公开接口与当前 slub.h 一致：

```c
void *slub_alloc(size_t size);
void slub_free(void *ptr);
void slub_check(void);
```

允许新增模块内 static 辅助函数、尺寸类表、页头和位图，不要求预设的内部函数名称。

[SPECIFICATION]

### slub_alloc

**Pre-Condition**：PMM 已初始化；请求为 size_t 值，元数据只在单核保护范围内访问。

**Post-Condition**：成功返回至少容纳 size 字节且 16 字节对齐的独占区域，存活对象之间不重叠；失败返回 NULL。

**Case 1**：小对象进入可容纳它的尺寸类；当前页无槽时可以申请新的底层页，页头不能占用对象槽。

**Case 2**：超出尺寸类的对象使用整页回退，记录用于整体归还的原始请求页数，兼容 Buddy。

**Case 3**：size 为 0、计算长度溢出或底层内存不足时返回 NULL，不泄漏已取得的页或破坏对象状态。

**Requirements**：避免 size 加页头或对齐运算的整数溢出；对象有效范围只按调用者请求界定。
### slub_free

**Pre-Condition**：ptr 为 NULL，或为本模块返回且尚未释放的对象首地址；接口不接受任意不可信地址。

**Post-Condition**：合法对象失去占用状态，其他存活对象不受影响；slab 全空或大对象释放时归还底层页。

**Case 1**：ptr 为 NULL 时无操作。

**Case 2**：合法小对象释放后槽可复用，最后一个对象释放后页计数恢复。

**Case 3**：合法大对象按保存的请求页数归还；同一 slab 的重复释放、错位内部指针须由断言拒绝。

**Requirements**：元数据更新保持单核中断保护，不声称具有 per-CPU、NUMA 或多核并发能力。
### slub_check

**Pre-Condition**：底层分配器处于可满足受控测试请求的状态。

**Post-Condition**：基本大小边界、对齐、写入读回、零长和溢出均被断言检查，全部对象释放后空闲页数恢复。

## 6. 选做 SLUB：独立测试提示词

[PROMPT]

**任务**：在 `tests/test_allocators.c` 的 objects 测试与负例入口、`kern/mm/slub.c` 的 slub_check 中补充对象行为测试，并复用 `tools/test_lab2.py` 的宿主及内核验收。

**操作要求**：必须修改实际测试文件；不改变 SLUB 分配策略来迎合用例，不删除既有断言。实现和测试保持独立，新增辅助函数为 static；保留其他模块测试。

**输出要求**：按规格交付两种底层管理器上的对象测试和非法释放负例，说明各类测试能发现的错误及其限制。

[RELY]

现有宿主适配层为 `tests/host_include/pmm.h`，有真实的 Page/PMM 声明和 host_ram 地址转换。测试中的固定环境摘录如下：

```c
#define NPAGES 512
static struct Page descriptors[NPAGES];
struct Page *pages=descriptors;
size_t npage=0x80000+NPAGES;
const size_t nbase=0x80000;
_Alignas(4096) unsigned char host_ram[NPAGES*PGSIZE];
```

`reset(const struct pmm_manager *m)` 初始化受控页区，alloc_pages/free_pages/nr_free_pages 调用被测管理器；测试模型必须独立于其空闲链表。宿主断言由 libc abort，内核断言使用现有 panic。

被测公开接口为：

```c
void *slub_alloc(size_t size);
void slub_free(void *ptr);
void slub_check(void);
```

`objects(0)` 使用 Best-Fit，`objects(1)` 使用 Buddy；主入口已有 slub-double、slub-interior 负例协议。对象大小上限、尺寸类和对齐约束与上一条实现规格一致。

[GUARANTEE]

```c
static void objects(int buddy);
int main(int argc, char **argv);
void slub_check(void);
```

保留 tests/test_allocators.c 的其他入口和 tools/test_lab2.py 的 host_tests() 调用。允许私有测试辅助函数，不新增公开分配接口。

[SPECIFICATION]

### objects

**Pre-Condition**：进入测试前无残留 SLUB 对象，可重置并选择 Best-Fit 或 Buddy。

**Post-Condition**：两种底层分别通过对象范围、内容、对齐和回收检查；每轮结束没有存活对象或页泄漏。

**Case 1**：覆盖所有尺寸类边界前后和较大对象；批量申请 300 个小对象验证跨 slab 扩展，释放部分槽后检查复用。

**Case 2**：每种底层执行 2500 次确定种子随机操作，对每个存活对象检查区间互斥并读回独立字节模式。

**Case 3**：回收后验证底层可用页数恢复；零长、溢出与 NULL 释放遵循接口语义。

**Requirements**：不依赖 SLUB 内部位图判定自身正确；sanitizer 与对象边界/数据模型互补，不能把二者覆盖范围混同。
### main 的 SLUB 负例入口

**Pre-Condition**：宿主进程独立运行已编译的测试程序，参数指定 slub-double 或 slub-interior。

**Post-Condition**：重复释放与错位内部指针分别触发预期 SIGABRT；正常测试不应崩溃或产生 sanitizer 错误。

**Requirements**：区分预期断言终止与其他异常；不在内核主测试中故意引发不可恢复 panic 来冒充负例统计。
### slub_check

**Pre-Condition**：当前 PMM 可满足尺寸边界和多页对象的受控申请。

**Post-Condition**：在真实 RISC-V 内核中验证尺寸、写入读回、对齐和最终页计数恢复；测试输出只在断言执行后产生。

## 7. MMU/TLB：QEMU 4.1.1 源码调试提示词

[PROMPT]

**任务**：维护 `kern/mm/mmu_probe.c`、`tools/mmu_trace.py`、`tools/build_debug_qemu.sh` 和 `docs/mmu-debugging.md`，使用与当前项目一致的 QEMU 4.1.1 完成 Sv39、TLB 与 Bare 源码调试。

**操作要求**：必须修改项目实际文件并保存原始记录。使用官方 4.1.1 源码和同版带符号可执行文件；版本不符时停止，不修改第三方源码或原始 PMM 检查。host GDB 调试 Linux ARM64 的 QEMU，guest GDB 面向 RISC-V 内核，二者不混用。

**输出要求**：交付能真实执行的 GDB 命令、调用栈、页表读取、PA 与 TLB 断言和结果摘要；全部日志记录实际版本与固件摘要，不能把旧 6.2.0 结果改标为 4.1.1。

[RELY]

现有 probe 变量和接口摘录为：

```c
volatile uintptr_t mmu_probe_value __attribute__((aligned(4096))) = 0x2413460;
uintptr_t __attribute__((noinline)) mmu_read_probe(void);
```

`pmm.h` 已有 `flush_tlb()`，执行带 memory clobber 的 sfence.vma。启动页表提供高地址 1 GiB 叶子映射。调试链涉及当前 QEMU 源码的 load_helper、riscv_cpu_tlb_fill、get_physical_address、tlb_set_page；真实读取类型为 MMU_DATA_LOAD，Bare 对照选 MMU_INST_FETCH。可用 gdb 和 nm；每轮须先按当前 Makefile 构建 bin/kernel，再读取其符号，现有 mmu_trace.py 会先执行构建。QEMU 源码及带符号可执行文件仍保留。

QEMU 4.1.1 的 get_physical_address 没有 is_debug 参数。应断言实际调用者为 riscv_cpu_tlb_fill，并且本轮不通过 guest GDB 查询目标内存；不能照抄 6.2.0 的断点条件。项目 BIOS 为 firmware/opensbi-v0.4-qemu-virt-fdt81f00000.bin，与正常启动保持一致。

[GUARANTEE]

```c
uintptr_t mmu_read_probe(void);
```

保留现有 Python 函数签名：

```python
def run(name, args, timeout=90):
```

容器中保留以下入口，运行前按 MMU 文档挂载 toolchains；可用 --bios 指定项目固件、--output 指定新的输出目录：

```bash
python3 tools/mmu_trace.py \
  --qemu /workspace/toolchains/qemu-4.1.1/build-lab2/qemu-system-riscv64 \
  --source /workspace/toolchains/qemu-4.1.1
```

输出 mmu-sv39.gdb、mmu-bare.gdb、两份原始日志和 mmu-results.json。

[SPECIFICATION]

### mmu_read_probe

**Pre-Condition**：CPU 已进入预期内核映射，probe 对象可读，PMM/启动映射状态有效。

**Post-Condition**：刷新翻译状态后实际读取同一 volatile 对象两次，断言值一致并返回读值。

**Requirements**：两个读取不能由编译器合并成一次或由 GDB 人工读内存代替；probe 放在已初始化、页对齐数据区。
### run(name,args,timeout=90)

**Pre-Condition**：参数为真实待执行命令，输出目录可写且需保留的旧证据已备份。

**Post-Condition**：在限时内执行真实子进程并保存合并输出；正常完成时返回其状态，超时或启动失败必须导致验证失败。

**Requirements**：不把退出异常写成通过；只收尾自己启动的调试进程。
### Sv39 调试流程

**Pre-Condition**：已核对 QEMU 与源码均为 4.1.1，内核 ELF 和 probe 符号可用；条件限定真实访问地址与类型，并验证调用者。

**Post-Condition**：记录真实 CPU 访存调用栈、TLB miss、satp、PTE 地址/值、实际输出 PA 及填充后的缓存状态，两次 load 的目标翻译次数符合观察。

**Case 1**：Sv39 命中根层合法大页叶子时解释为何提前结束，不能伪写为必经三级。

**Case 2**：翻译或缓存断言不成立、符号缺失、源码版本不匹配或命令未执行时保留失败记录，不生成 passed=true。

**Requirements**：目标地址从当前 ELF 获取；页表手算仅用于核对，不冒充 QEMU 的实际翻译。
### Bare 对照与结果汇总

**Pre-Condition**：选定内核第一条取指的实际地址，调试时 satp 为 0。

**Post-Condition**：记录 Bare 下 VA/PA 相等的真实返回，并说明软件 TLB 仍可服务 guest 到宿主内存访问；汇总按每项退出状态和断言决定通过。

**Requirements**：主动结束 Bare 被调试进程属于脚本收尾，应与内核崩溃区分；不声称测量了硬件 TLB 命中率或替换策略。

## 8. 硬件可用内存范围思考题

[PROMPT]

**任务**：在 `report/report.md` 第 4.3 节解释 OS 未预先获知 RAM 范围时的资源发现方法，并与本实验 DTB 实现对照。

**操作要求**：必须修改真实报告文件；本题为原理分析，不修改驱动、固件或页分配器，不通过实际写未知地址做破坏性探测。

**输出要求**：按照下列可信上下文和规格完成回答，区分通用原理、架构专用机制与本实验已实现行为。

[RELY]

当前设备树公开接口取自 dtb.h：

```c
int dtb_parse_memory(const void *blob, size_t limit, uintptr_t *base, uintptr_t *size);
void dtb_init(uintptr_t hartid, uintptr_t dtb_pa);
uintptr_t get_memory_base(void);
uintptr_t get_memory_size(void);
```

当前内核针对 QEMU virt 的单个 DRAM bank，设备树物理指针由 OpenSBI 交接；当前布局会扣除固件、内核和描述符占用。UEFI 内存映射和 x86 E820 是对照机制，不是此 RISC-V 工程已实现的接口。

[GUARANTEE]

交付报告 4.3 节的资源发现途径、可用/保留/MMIO 区分及限制说明。本题不新增 C 函数，现有 dtb 接口只作为分析依据。

[SPECIFICATION]

### 固件与平台交接方法的报告回答

**Pre-Condition**：已知当前工程如何获得 DTB，能够区分平台资源描述和实际可分配页。

**Post-Condition**：解释 DTB、UEFI、E820、ACPI/板级信息的适用条件，说明内存范围还需排除保留、设备和已占用区域后按完整页管理。

**Requirements**：不把某一架构 API 描述为所有平台通用调用，也不将信息表中的每段地址都判为可用 RAM。
### 主动探测与本实验限制的报告回答

**Pre-Condition**：回答以现有实现和已确认的平台约定为依据。

**Post-Condition**：说明盲写未知地址可能破坏 MMIO 或固件数据；即使读回一致，仍需依据资源描述和保留区信息判断可分配性。明确当前实现仅处理 QEMU virt 单 DRAM bank，不提供通用 reserved-memory 管理。

## 9. QEMU 4.1.1 固件配置、验收与原生终端证据

[PROMPT]

**任务**：维护 `report/capture-terminal.sh`、`tools/build_qemu411_firmware.sh`、采集说明和报告环境/测试章节，在项目 QEMU 4.1.1 下完成固件边界修正、验收及原生截图采集。

**操作要求**：必须操作实际脚本和文档，执行前核对 Compose、Makefile、QEMU 版本与日志位置并备份。不得修改内核分配算法、原始测试断言或第三方源码来使验收通过；固件地址通过上游构建参数调整。不执行 Git 提交、推送或全局 Docker 清理。

**输出要求**：保留原默认固件失败记录和修正配置后的实际结果，交付 QEMU/固件版本、摘要、原始日志、退出码及七张原生终端 PNG，并同步报告引用。

[RELY]

Compose 服务 oslab 使用 QEMU 4.1.1；项目 Makefile 的 BIOS 默认指向 firmware/opensbi-v0.4-qemu-virt-fdt81f00000.bin。原 OpenSBI 0.4 的 FDT 目标为 0x82200000，超出 32 MiB RAM；构建参数改为 0x81f00000，支持本地 32/64/128/256 MiB 矩阵。QEMU 源码及带符号程序在 docker/toolchains/qemu-4.1.1/，源码调试镜像为 nku-oslab:qemu4.1.1-debug；仍需查询实际 --version。make grade 先构建宿主程序与内核，截图脚本先备份，工具链目录另行挂载。

[GUARANTEE]

保留现有 Bash 函数及参数协议：

```bash
run_test()      # name 后接实际命令及参数
show_output()   # name、title、lines
capture_window() # 输出图片名，不含扩展名
```

交付带时间记录的命令、退出码、完整日志和七张原生终端 PNG。固件附带未修改的上游许可证、来源说明和 SHA-256；主图与原件保持逐字节一致。保留 run_test、show_output、capture_window 的参数协议，允许新增私有显示辅助函数。

[SPECIFICATION]

### run_test

**Pre-Condition**：实际命令、工作环境和证据输出位置已确认，旧记录已备份。

**Post-Condition**：成功时保存原始完整输出、命令和真实退出码；失败时保留错误并停止成功截图流程。

**Case 1**：环境可运行时执行现有检查，而不预填成功结果。

**Case 2**：不同 QEMU/固件行为不一致时保留两种环境的实际输出，区分已证实差异和待定位原因。

**Requirements**：已有验收入口具有其超时控制；不删除失败用例或将所有非零状态都归为预期成功。
### show_output

**Pre-Condition**：run_test 已留下对应的真实命令、时间、状态和日志。

**Post-Condition**：在终端显示本轮元数据及原始输出；展示末尾或关键段时标明摘录，完整日志仍保留。

**Requirements**：不改写 PASS、地址、数字或错误信息来美化结果。
### capture_window

**Pre-Condition**：用户在 macOS 原生终端运行采集脚本，真实测试结果已可见，窗口截图权限可用。

**Post-Condition**：调用系统窗口截图保存实际终端 PNG；取消或无有效图片时明确失败，不能当作完成。

**Requirements**：不绘制终端，不以浏览器控制台图片代替原生窗口；核验后再按用户授权更新报告图片，保持原件与副本校验一致。
### 固件构建与版本核对

**Pre-Condition**：官方 QEMU 4.1.1 归档及其 OpenSBI 0.4 源码可校验，RISC-V 工具链可用，构建输出与源码分离。

**Post-Condition**：通过上游 FW_JUMP_FDT_ADDR 构建参数生成 DTB 位于 0x81f00000 的同版固件，保留 0x80200000 内核入口，附许可证、来源和摘要。QEMU 可执行文件或源码版本不符时停止，不能生成成功结果。

**Requirements**：不编辑第三方源码；使用裸机编译选项避免 GCC 将自定义内存函数优化为外部 libc 调用。记录修正前后固件差异，保留原失败用例及其日志。

### 证据与报告对照

**Pre-Condition**：各项原始命令与证据已取得，来源和日期可核查。

**Post-Condition**：正文、图片、日志与校验记录一致，分别说明 AI 执行、用户脚本复跑和未确认的成员独立审核。

**Requirements**：代码或图片变化后更新相关文档校验，历史快照保留原有归属；本地验收不称为官方成绩。
