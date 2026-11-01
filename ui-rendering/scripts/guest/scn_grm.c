/* Scenario plugin: hook libObj GRM_gperm* via GOT slots and log parameters. */
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

static int (*real_fill)(u32, u32, const u32 *, u32);
static int (*real_blit)(u32, u32, const u32 *, u32);
static int fill_n, blit_n;

static int hook_fill(u32 a0, u32 a1, const u32 *p, u32 a3)
{
    if (fill_n < 24 && p) {
        u32 v[14];
        int i;
        v[0] = a0; v[1] = a1; v[2] = a3; v[3] = (u32)p;
        for (i = 0; i < 10; i++) v[4 + i] = p[i];
        hexline("GRMF", v, 14);
        fill_n++;
    }
    return real_fill ? real_fill(a0, a1, p, a3) : 5;
}
static int hook_blit(u32 a0, u32 a1, const u32 *p, u32 a3)
{
    if (blit_n < 24 && p) {
        u32 v[20];
        int i;
        v[0] = a0; v[1] = a1; v[2] = a3; v[3] = (u32)p;
        for (i = 0; i < 16; i++) v[4 + i] = p[i];
        hexline("GRMB", v, 20);
        blit_n++;
    }
    return real_blit ? real_blit(a0, a1, p, a3) : 5;
}

int scenario_run(u32 data, u32 size, u32 arg2)
{
    void *h;
    int (*ov)(const char *, int);
    int r;
    u32 base = 0;
    volatile u32 *slot;
    (void)data; (void)size; (void)arg2;
    slog("SCN enter\n");
    h = dlopen("/usr/lib/libObj.so", 1);
    real_fill = (int (*)(u32, u32, const u32 *, u32))dlsym(h, "GRM_gpermFillrect");
    real_blit = (int (*)(u32, u32, const u32 *, u32))dlsym(h, "GRM_gpermRectblit");
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    dlopen("/tmp/v.so", 1);
    if (!real_fill || !real_blit || !ov) { slog("SCN missing\n"); return 0; }
    {
        struct dlinfo di;
        if (dladdr((void *)((u32)real_blit & ~1u), &di)) base = (u32)di.fbase;
    }
    {
        u32 v[4];
        v[0] = (u32)real_fill; v[1] = (u32)real_blit; v[2] = base; v[3] = (u32)ov;
        hexline("SCNG", v, 4);
    }
    if (base) {
        u32 p1 = (base + 0x126af2c) & ~0xfffu;
        u32 p2 = (base + 0x126c460) & ~0xfffu;
        u32 a = (u32)hook_fill, b = (u32)hook_blit;
        u32 mv[4];
        mv[0] = (u32)sys3(125, p1, 0x1000, 3);
        mv[1] = (u32)sys3(125, p2, 0x1000, 3);
        slot = (volatile u32 *)(base + 0x126af2c);
        *slot = a;
        slot = (volatile u32 *)(base + 0x126c460);
        *slot = b;
        mv[2] = a; mv[3] = b;
        hexline("SCNP", mv, 4);
    }
    r = ov("view/BASE_MENU", 0);
    {
        u32 v[2];
        v[0] = (u32)r; v[1] = (u32)(fill_n << 16 | (blit_n & 0xffff));
        hexline("SCNR", v, 2);
    }
    slog("SCN done\n");
    return 0;
}
