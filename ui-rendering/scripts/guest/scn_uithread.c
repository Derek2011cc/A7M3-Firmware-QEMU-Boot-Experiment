/* Scenario plugin: install an inline hook on libObj's 2D scheduler and call
 * viewManagerIf::openView from inside the hook (i.e. from the app's own
 * rendering/UI thread) instead of from the testcmd thread.
 * View name is read from /tmp/viewlist (first line, without "view/"). */
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
static void hexlog(const char *tag, u32 v)
{
    static const char d[] = "0123456789abcdef";
    char b[64]; int n = 0, i;
    while (tag[n] && n < 24) { b[n] = tag[n]; n++; }
    for (i = 28; i >= 0; i -= 4) b[n++] = d[(v >> i) & 15];
    b[n++] = '\n';
    sys3(4, 2, (long)b, n);
}
static void cacheflush(u32 s, u32 e)
{
    sys3(0x0f0002, s, e, 0);
}

static int (*tramp2d)(void *);
static volatile int n2d;
static volatile int busy;
static int (*g_ov)(const char *, int);

static void try_open(void)
{
    char list[256];
    char path[96];
    int fd, n, i, p = 0;
    if (!g_ov || busy) return;
    busy = 1;
    slog("UIH try\n");
    fd = (int)sys3(5, (long)"/tmp/viewlist", 0, 0);
    if (fd < 0) { slog("UIH no list\n"); busy = 0; return; }
    n = (int)sys3(3, fd, (long)list, (long)sizeof(list) - 1);
    sys3(6, fd, 0, 0);
    if (n <= 0) { slog("UIH empty\n"); busy = 0; return; }
    list[n] = 0;
    for (i = 0; i < 5; i++) path[p++] = "view/"[i];
    for (i = 0; i < n && list[i] != '\n' && list[i] != '\r' && p < 90; i++)
        path[p++] = list[i];
    path[p] = 0;
    hexlog("UIH name ", (u32)p);
    {
        int r = g_ov(path, 0);
        hexlog("UIH ret  ", (u32)r);
    }
    busy = 0;
}

static int hook2d(void *p)
{
    if (n2d < 4) {
        hexlog("UIH hook ", (u32)p);
        n2d++;
        try_open();
    }
    return tramp2d ? tramp2d(p) : 0;
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

int scenario_run(u32 data, u32 size, u32 arg2)
{
    void *h;
    int (*ov)(const char *, int);
    u32 base = 0;
    (void)data; (void)size; (void)arg2;
    slog("UIH enter\n");
    h = dlopen("/usr/lib/libObj.so", 1);
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    if (!ov) { slog("UIH missing\n"); return 0; }
    g_ov = ov;
    {
        struct dlinfo di;
        if (dladdr((void *)((u32)ov & ~1u), &di)) base = (u32)di.fbase;
    }
    if (base) {
        u32 t2 = base + 0x73a100u;
        u32 r;
        hexlog("UIH t2  ", t2);
        hexlog("UIH orig", *(volatile u32 *)t2);
        tramp2d = 0;
        r = 0;
        if (*(volatile u32 *)t2 == 0x41f0e92du) {
            u32 tramp = build_tramp(t2, 8, (const unsigned char *)t2);
            if (tramp) {
                tramp2d = (int (*)(void *))tramp;
                if (sys3(125, t2 & ~0xfffu, 0x1000, 7) == 0) {
                    volatile u32 *patch = (volatile u32 *)t2;
                    patch[0] = 0xf000f8dfu;
                    patch[1] = (u32)hook2d;
                    cacheflush(t2, t2 + 8);
                    sys3(125, t2 & ~0xfffu, 0x1000, 5);
                    r = 1;
                }
            }
        }
        hexlog("UIH inst ", r);
    }
    slog("UIH done\n");
    return 0;
}
