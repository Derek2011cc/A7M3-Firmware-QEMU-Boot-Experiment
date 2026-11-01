/* Scenario plugin: inline-hook libObj GPE submit functions and log jobs. */
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
static void cacheflush(u32 s, u32 e)
{
    sys3(0x0f0002, s, e, 0);
}

static char mapsbuf[262144];
static int maplen;
static void load_maps(void)
{
    int fd = (int)sys3(5, (long)"/proc/self/maps", 0, 0);
    maplen = 0;
    if (fd < 0) return;
    while (maplen < (int)sizeof(mapsbuf) - 1) {
        long n = sys3(3, fd, (long)(mapsbuf + maplen), sizeof(mapsbuf) - 1 - maplen);
        if (n <= 0) break;
        maplen += (int)n;
    }
    sys3(6, fd, 0, 0);
    mapsbuf[maplen] = 0;
}
static void dump_maps(void)
{
    int i = 0;
    slog("MAPSB\n");
    while (i < maplen) {
        int j = i;
        while (j < maplen && mapsbuf[j] != '\n') j++;
        if (j > i) {
            sys3(4, 2, (long)(mapsbuf + i), j - i);
            sys3(4, 2, (long)"\n", 1);
        }
        i = j + 1;
    }
    slog("MAPSE\n");
}

static int (*tramp2d)(void *);
static int (*trampkick)(void *);
static volatile int n2d, nkick;

static int hook2d(void *p)
{
    if (n2d < 48 && p) {
        u32 v[14]; int i;
        v[0] = (u32)p;
        for (i = 0; i < 13; i++) v[1 + i] = ((const u32 *)p)[i];
        hexline("H2D", v, 14);
        n2d++;
    }
    return tramp2d ? tramp2d(p) : 0;
}
static int hookkick(void *p)
{
    if (nkick < 48 && p) {
        u32 v[6]; int i;
        v[0] = (u32)p;
        for (i = 0; i < 5; i++) v[1 + i] = ((const u32 *)p)[i];
        hexline("HKICK", v, 6);
        nkick++;
    }
    return trampkick ? trampkick(p) : 0;
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
    if (off != nbytes) { p[nbytes] = 0x00; p[nbytes + 1] = 0xbf; }
    ins = (volatile u32 *)(page + off);
    ins[0] = 0xf000f8dfu;
    ins[1] = (target + nbytes) | 1u;
    cacheflush(page, page + 0x40);
    sys3(125, page, 0x1000, 5);
    return page | 1u;
}

/* which: 1 = 2D scheduler (0x73a100), 2 = 0x1800 kick (0x73a290) */
static u32 install(u32 target, u32 nbytes, void *hookaddr, u32 expect, int which)
{
    const unsigned char *orig = (const unsigned char *)target;
    u32 tramp;
    volatile u32 *patch;
    if (*(volatile u32 *)target != expect) return 0;
    tramp = build_tramp(target, nbytes, orig);
    if (!tramp) return 0;
    if (which == 1) tramp2d = (int (*)(void *))tramp;
    if (which == 2) trampkick = (int (*)(void *))tramp;
    if (sys3(125, target & ~0xfffu, 0x1000, 7) != 0) return 0;
    patch = (volatile u32 *)target;
    patch[0] = 0xf000f8dfu;
    patch[1] = (u32)hookaddr;
    if (nbytes == 10) ((volatile unsigned short *)target)[4] = 0xbf00;
    cacheflush(target, target + nbytes);
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
    load_maps();
    dump_maps();
    h = dlopen("/usr/lib/libObj.so", 1);
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    if (!ov) { slog("SCN missing\n"); return 0; }
    dlopen("/tmp/v.so", 1);
    {
        struct dlinfo di;
        if (dladdr((void *)((u32)ov & ~1u), &di)) base = (u32)di.fbase;
    }
    if (base) {
        u32 v[6], t2, tk;
        t2 = base + 0x73a100u;
        tk = base + 0x73a290u;
        v[0] = base; v[1] = t2; v[2] = *(volatile u32 *)t2;
        v[3] = tk; v[4] = *(volatile u32 *)tk; v[5] = (u32)hook2d;
        hexline("HINFO", v, 6);
        tramp2d = 0; trampkick = 0;
        {
            u32 r1 = install(t2, 8, (void *)hook2d, 0x41f0e92du, 1);
            u32 r2 = install(tk, 10, (void *)hookkick, 0x4604b5b0u, 2);
            u32 w[4];
            w[0] = r1; w[1] = r2;
            w[2] = (u32)tramp2d; w[3] = (u32)trampkick;
            hexline("HINST", w, 4);
        }
    }
    ov("view/BASE_MENU", 0);
    {
        u32 w[2];
        w[0] = (u32)n2d; w[1] = (u32)nkick;
        hexline("HDONE", w, 2);
    }
    slog("SCN done\n");
    return 0;
}
