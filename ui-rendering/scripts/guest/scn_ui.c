/* Scenario plugin: hook GraphicsImage clear/copy impls and gperm, log maps. */
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
static void cacheflush_(u32 s, u32 e) { sys3(0x0f0002, s, e, 0); }
static u32 rd(u32 a) { return (a > 0x1000 && a < 0x60000000) ? *(volatile u32 *)a : 0; }

static char mapsbuf[131072];
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
static void log_views(const char *when)
{
    int i = 0;
    slog(when); slog(" MAPS\n");
    while (i < maplen) {
        int j = i;
        while (j < maplen && mapsbuf[j] != '\n') j++;
        {
            int k, hit = 0;
            for (k = i; k + 3 < j; k++) {
                if (mapsbuf[k] == 'v' && mapsbuf[k+1] == 'i' && mapsbuf[k+2] == 'e' && mapsbuf[k+3] == 'w') hit = 1;
                if (mapsbuf[k] == 'l' && mapsbuf[k+1] == 'i' && mapsbuf[k+2] == 'b' && mapsbuf[k+3] == 'O') hit = 1;
            }
            if (hit && mapsbuf[i] != 0) {
                char b[180]; int n = 0, p;
                for (p = i; p < j && n < 170; p++) {
                    char c = mapsbuf[p];
                    if (c != 0) b[n++] = c;
                }
                b[n++] = '\n';
                sys3(4, 2, (long)b, n);
            }
        }
        i = j + 1;
    }
    slog("MAPSEND\n");
}

static int (*t_clear)(void *, void *, int, int, int, int);
static int (*t_copy)(void *, void *, void *, void *, void *, void *, void *,
                     void *, void *, void *);
static int (*t_fill)(void *, void *, void *, void *, void *, void *);
static int (*t_rect)(void *, void *, void *, void *, void *, void *);
static int (*t_2d)(void *);
static volatile int nclear, ncopy, nfill, nrect, n2d;

static int h_2d(void *p)
{
    n2d++;
    if (n2d <= 4 || (n2d % 16) == 0) {
        u32 v[10]; int i;
        v[0] = (u32)p;
        for (i = 0; i < 8; i++) v[1 + i] = p ? ((const u32 *)p)[i] : 0;
        v[9] = (u32)n2d;
        hexline("UI2D", v, 10);
        load_maps();
        log_views("DRAW");
    }
    return t_2d ? t_2d(p) : 0;
}

static int h_clear(void *img, void *color, int a, int b, int c, int d)
{
    if (nclear < 24) {
        u32 v[14];
        v[0] = (u32)img; v[1] = (u32)color; v[2] = (u32)a; v[3] = (u32)b;
        v[4] = (u32)c; v[5] = (u32)d;
        v[6] = rd((u32)img); v[7] = rd((u32)img + 0x18);
        v[8] = rd(rd((u32)img + 0x18));
        v[9] = rd(rd((u32)img + 0x18) + 4);
        v[10] = rd(rd((u32)img + 0x18) + 8);
        v[11] = rd(rd((u32)img + 0x18) + 0xc);
        v[12] = color ? *(volatile unsigned char *)color : 0;
        v[13] = (u32)nclear;
        hexline("UICLR", v, 14);
        nclear++;
    }
    return t_clear ? t_clear(img, color, a, b, c, d) : 0;
}
static int h_copy(void *a0, void *a1, void *a2, void *a3, void *a4,
                  void *a5, void *a6, void *a7, void *a8, void *a9)
{
    if (ncopy < 24) {
        u32 v[14];
        v[0] = (u32)a0; v[1] = (u32)a1; v[2] = (u32)a2; v[3] = (u32)a3;
        v[4] = (u32)a4; v[5] = (u32)a5; v[6] = (u32)a6; v[7] = (u32)a7;
        v[8] = (u32)a8; v[9] = (u32)a9;
        v[10] = rd((u32)a0); v[11] = rd((u32)a0 + 0x18);
        v[12] = rd(rd((u32)a0 + 0x18));
        v[13] = (u32)ncopy;
        hexline("UICPY", v, 14);
        ncopy++;
    }
    return t_copy ? t_copy(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) : 0;
}
static int h_fill(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5)
{
    if (nfill < 24) {
        u32 v[10];
        v[0] = (u32)a0; v[1] = (u32)a1; v[2] = (u32)a2; v[3] = (u32)a3;
        v[4] = (u32)a4; v[5] = (u32)a5;
        v[6] = rd((u32)a2); v[7] = rd((u32)a2 + 4);
        v[8] = rd((u32)a2 + 8); v[9] = (u32)nfill;
        hexline("UIFILL", v, 10);
        nfill++;
    }
    return t_fill ? t_fill(a0, a1, a2, a3, a4, a5) : 0;
}
static int h_rect(void *a0, void *a1, void *a2, void *a3, void *a4, void *a5)
{
    if (nrect < 24) {
        u32 v[10];
        v[0] = (u32)a0; v[1] = (u32)a1; v[2] = (u32)a2; v[3] = (u32)a3;
        v[4] = (u32)a4; v[5] = (u32)a5;
        v[6] = rd((u32)a2); v[7] = rd((u32)a2 + 0xc);
        v[8] = 0; v[9] = (u32)nrect;
        hexline("UIRECT", v, 10);
        nrect++;
    }
    return t_rect ? t_rect(a0, a1, a2, a3, a4, a5) : 0;
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
    cacheflush_(page, page + 0x40);
    sys3(125, page, 0x1000, 5);
    return page | 1u;
}
static u32 install(u32 target, u32 nbytes, void *hookaddr, u32 expect, int which)
{
    const unsigned char *orig = (const unsigned char *)target;
    u32 tramp;
    volatile u32 *patch;
    if (*(volatile u32 *)target != expect) return 0;
    tramp = build_tramp(target, nbytes, orig);
    if (!tramp) return 0;
    if (which == 1) t_clear = (int (*)(void *, void *, int, int, int, int))tramp;
    if (which == 2) t_copy = (int (*)(void *, void *, void *, void *, void *,
                                      void *, void *, void *, void *, void *))tramp;
    if (which == 3) t_fill = (int (*)(void *, void *, void *, void *, void *, void *))tramp;
    if (which == 4) t_rect = (int (*)(void *, void *, void *, void *, void *, void *))tramp;
    if (which == 5) t_2d = (int (*)(void *))tramp;
    if (sys3(125, target & ~0xfffu, 0x1000, 7) != 0) return 0;
    patch = (volatile u32 *)target;
    patch[0] = 0xf000f8dfu;
    patch[1] = (u32)hookaddr;
    if (nbytes == 10) ((volatile unsigned short *)target)[4] = 0xbf00;
    cacheflush_(target, target + nbytes);
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
    load_maps();
    log_views("BEFORE");
    if (base) {
        u32 w[10];
        u32 rc, rp, rf, rr;
        u32 tc = base + 0x611590u, tp = base + 0x6119bcu;
        u32 tf = base + 0x64ea98u, tr = base + 0x64ebd2u;
        u32 t2 = base + 0x73a100u;
        w[0] = base; w[1] = tc; w[2] = *(volatile u32 *)tc; w[3] = tp;
        w[4] = *(volatile u32 *)tp; w[5] = *(volatile u32 *)tf;
        w[6] = *(volatile u32 *)tr; w[7] = *(volatile u32 *)t2;
        w[8] = 0; w[9] = 0;
        hexline("UIINFO", w, 10);
        t_clear = 0; t_copy = 0; t_fill = 0; t_rect = 0; t_2d = 0;
        rc = install(tc, 8, (void *)h_clear, 0xb089b5f0u, 1);
        rp = install(tp, 8, (void *)h_copy, 0x4ff0e92du, 2);
        rf = install(tf, 8, (void *)h_fill, 0x4ff0e92du, 3);
        rr = install(tr, 8, (void *)h_rect, 0x4ff0e92du, 4);
        {
            u32 r2 = install(t2, 8, (void *)h_2d, 0x41f0e92du, 5);
            w[0] = rc; w[1] = rp; w[2] = rf; w[3] = rr;
            w[4] = r2;
            w[5] = (u32)t_clear; w[6] = (u32)t_copy; w[7] = (u32)t_fill;
            w[8] = (u32)t_rect; w[9] = (u32)t_2d;
        }
        hexline("UIINST", w, 10);
    }
    ov("view/BASE_MENU", 0);
    load_maps();
    log_views("AFTER");
    {
        u32 w[6];
        w[0] = (u32)nclear; w[1] = (u32)ncopy; w[2] = (u32)nfill; w[3] = (u32)nrect;
        w[4] = (u32)n2d; w[5] = 0;
        hexline("UIDONE", w, 6);
    }
    slog("SCN done\n");
    return 0;
}
