/* Scenario plugin: inline-hook GRM_gpermFillrect/Rectblit and log params. */
typedef unsigned int u32;
typedef int s32;
extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *handle, const char *name);
extern int dladdr(void *addr, void *info);
struct dlinfo { const char *fname; void *fbase; const char *sname; void *saddr; };

static long sys3(long n, long a, long b, long c)
{
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r7 __asm__("r7") = n;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
static long sys6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r3 __asm__("r3") = d;
    register long r4 __asm__("r4") = e;
    register long r5 __asm__("r5") = f;
    register long r7 __asm__("r7") = n;
    __asm__ volatile("svc #0" : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5), "r"(r7)
                     : "memory");
    return r0;
}
static void slog(const char *t)
{
    int n = 0;
    while (t[n]) n++;
    sys3(4, 2, (long)t, n);
}
static void hexline(const char *tag, const u32 *v, int n)
{
    static const char d[] = "0123456789abcdef";
    char b[320]; int p = 0, i, j;
    while (tag[p] && p < 20) { b[p] = tag[p]; p++; }
    for (i = 0; i < n; i++) {
        b[p++] = ' ';
        for (j = 28; j >= 0; j -= 4) b[p++] = d[(v[i] >> j) & 15];
    }
    b[p++] = '\n';
    sys3(4, 2, (long)b, p);
}
static void cacheflush(u32 s, u32 e) { sys3(0x0f0002, s, e, 0); }

static int (*trampfill)(void *, void *, void *, void *);
static int (*trampblit)(void *, void *, void *, void *);
static volatile int nfill, nblit;

static void dump_call(const char *tag, int n, void *a0, void *a1, const void *p, void *a3)
{
    u32 v[24]; int i;
    v[0] = (u32)a0; v[1] = (u32)a1; v[2] = (u32)p; v[3] = (u32)a3;
    for (i = 0; i < 20; i++) v[4 + i] = p ? ((const u32 *)p)[i] : 0;
    hexline(tag, v, 24);
}
static int hookfill(void *a0, void *a1, void *p, void *a3)
{
    if (nfill < 24) { dump_call("HFILL", nfill, a0, a1, p, a3); nfill++; }
    return trampfill ? (int)trampfill(a0, a1, p, a3) : 0;
}
static int hookblit(void *a0, void *a1, void *p, void *a3)
{
    if (nblit < 24) { dump_call("HBLIT", nblit, a0, a1, p, a3); nblit++; }
    return trampblit ? (int)trampblit(a0, a1, p, a3) : 0;
}

static u32 build_tramp(u32 target, u32 nbytes, const unsigned char *orig)
{
    u32 page = (u32)sys6(192, 0, 0x1000, 7, 0x22, -1, 0);
    unsigned char *p;
    u32 off, i;
    volatile u32 *ins;
    if (page < 0x1000 || page > 0x60000000) return 0;
    p = (unsigned char *)page;
    for (i = 0; i < nbytes; i++) p[i] = orig[i];
    off = (nbytes + 3) & ~3u;
    ins = (volatile u32 *)(page + off);
    ins[0] = 0xf000f8dfu;
    ins[1] = (target + nbytes) | 1u;
    cacheflush(page, page + 0x40);
    sys3(125, page, 0x1000, 5);
    return page | 1u;
}
static u32 install(u32 target, void *hookaddr, u32 expect, int which)
{
    const unsigned char *orig = (const unsigned char *)target;
    u32 tramp;
    volatile u32 *patch;
    if (*(volatile u32 *)target != expect) return 0;
    tramp = build_tramp(target, 8, orig);
    if (!tramp) return 0;
    if (which == 1) trampfill = (int (*)(void *, void *, void *, void *))tramp;
    if (which == 2) trampblit = (int (*)(void *, void *, void *, void *))tramp;
    if (sys3(125, target & ~0xfffu, 0x1000, 7) != 0) return 0;
    patch = (volatile u32 *)target;
    patch[0] = 0xf000f8dfu;
    patch[1] = (u32)hookaddr;
    cacheflush(target, target + 8);
    sys3(125, target & ~0xfffu, 0x1000, 5);
    return tramp;
}

int scenario_run(u32 data, u32 size, u32 arg2)
{
    void *h;
    int (*ov)(const char *, int);
    u32 base = 0;
    (void)data; (void)size; (void)arg2;
    slog("SCN enter\n");
    h = dlopen("/usr/lib/libObj.so", 1);
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    if (!ov) { slog("SCN missing\n"); return 0; }
    dlopen("/tmp/v.so", 1);
    {
        struct dlinfo di;
        if (dladdr((void *)((u32)ov & ~1u), &di)) base = (u32)di.fbase;
    }
    if (base) {
        u32 v[6], tf = base + 0x64ea98u, tb = base + 0x64ebd2u;
        u32 rf, rb, ga;
        v[0] = base; v[1] = tf; v[2] = *(volatile u32 *)tf;
        v[3] = tb; v[4] = *(volatile u32 *)tb; v[5] = 0;
        hexline("GINFO", v, 6);
        trampfill = 0; trampblit = 0;
        rf = install(tf, (void *)hookfill, 0x4ff0e92du, 1);
        rb = install(tb, (void *)hookblit, 0x4ff0e92du, 2);
        ga = (u32)dlsym(h, "GRM_bitmapGetPhysicalAddress");
        v[0] = rf; v[1] = rb; v[2] = (u32)trampfill; v[3] = (u32)trampblit;
        v[4] = ga; v[5] = (u32)hookfill;
        hexline("GINST", v, 6);
    }
    ov("view/BASE_MENU", 0);
    {
        u32 w[2];
        w[0] = (u32)nfill; w[1] = (u32)nblit;
        hexline("GDONE", w, 2);
    }
    slog("SCN done\n");
    return 0;
}
