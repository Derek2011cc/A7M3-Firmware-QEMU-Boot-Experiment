/* Scenario plugin: inline-hook GraphicsImage clear/copy implementations. */
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
static u32 rd(u32 a) { return (a > 0x1000 && a < 0x60000000) ? *(volatile u32 *)a : 0; }

static int (*tramp_clear)(void *, void *, int, int, int, int);
static int (*tramp_copy)(void *, void *, void *, void *, void *, void *, void *,
                         void *, void *, void *);
static volatile int nclear, ncopy;

static int hook_clear(void *img, void *color, int a, int b, int c, int d)
{
    if (nclear < 48) {
        u32 v[12];
        v[0] = (u32)img; v[1] = (u32)color; v[2] = (u32)a; v[3] = (u32)b;
        v[4] = (u32)c; v[5] = (u32)d;
        v[6] = rd((u32)img); v[7] = rd((u32)img + 0x18);
        v[8] = rd(rd((u32)img + 0x18)); v[9] = rd(rd((u32)img + 0x18) + 4);
        v[10] = rd(rd((u32)img + 0x18) + 8); v[11] = rd(rd((u32)img + 0x18) + 0xc);
        hexline("GICLEAR", v, 12);
        nclear++;
    }
    return tramp_clear ? tramp_clear(img, color, a, b, c, d) : 0;
}
static int hook_copy(void *a0, void *a1, void *a2, void *a3, void *a4,
                     void *a5, void *a6, void *a7, void *a8, void *a9)
{
    if (ncopy < 48) {
        u32 v[12];
        v[0] = (u32)a0; v[1] = (u32)a1; v[2] = (u32)a2; v[3] = (u32)a3;
        v[4] = (u32)a4; v[5] = (u32)a5; v[6] = (u32)a6; v[7] = (u32)a7;
        v[8] = (u32)a8; v[9] = (u32)a9;
        v[10] = rd((u32)a0); v[11] = rd((u32)a0 + 0x18);
        hexline("GICOPY", v, 12);
        ncopy++;
    }
    return tramp_copy ? tramp_copy(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) : 0;
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
    if (which == 1) tramp_clear = (int (*)(void *, void *, int, int, int, int))tramp;
    if (which == 2) tramp_copy = (int (*)(void *, void *, void *, void *, void *,
                                          void *, void *, void *, void *, void *))tramp;
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
        u32 tc = base + 0x611590u, tp = base + 0x6119bcu;
        u32 rc, rp;
        u32 v[6];
        v[0] = base; v[1] = tc; v[2] = *(volatile u32 *)tc;
        v[3] = tp; v[4] = *(volatile u32 *)tp; v[5] = (u32)hook_clear;
        hexline("GIINFO", v, 6);
        tramp_clear = 0; tramp_copy = 0;
        rc = install(tc, (void *)hook_clear, 0xb084b5f0u, 1);
        rp = install(tp, (void *)hook_copy, 0x4ff0e92du, 2);
        v[0] = rc; v[1] = rp; v[2] = (u32)tramp_clear; v[3] = (u32)tramp_copy;
        v[4] = 0; v[5] = 0;
        hexline("GIINST", v, 6);
    }
    ov("view/BASE_MENU", 0);
    {
        u32 w[2];
        w[0] = (u32)nclear; w[1] = (u32)ncopy;
        hexline("GIDONE", w, 2);
    }
    slog("SCN done\n");
    return 0;
}
