/* Scenario plugin: hook osal_rcv_msg_tmo (the app's message loop) and call
 * viewManagerIf::openView from inside that hook, i.e. from the thread that
 * runs the app's message loop.  View name: /tmp/viewlist (first line). */
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
static void slog(const char *t) { int n=0; while(t[n]) n++; sys3(4,2,(long)t,n); }
static void hexlog(const char *tag, u32 v)
{
    static const char d[] = "0123456789abcdef";
    char b[64]; int n = 0, i;
    while (tag[n] && n < 24) { b[n] = tag[n]; n++; }
    for (i = 28; i >= 0; i -= 4) b[n++] = d[(v >> i) & 15];
    b[n++] = '\n';
    sys3(4, 2, (long)b, n);
}
static void cacheflush(u32 s, u32 e) { sys3(0x0f0002, s, e, 0); }

static int (*tramp_rcv)(u32, void *, int);
static volatile int busy, tried;
static int (*g_ov)(const char *, int);

static void try_open(void)
{
    char list[256]; char path[96];
    int fd, n, i, p = 0;
    if (!g_ov || busy) return;
    busy = 1;
    slog("UIL try\n");
    fd = (int)sys3(5, (long)"/tmp/viewlist", 0, 0);
    if (fd < 0) { slog("UIL no list\n"); busy = 0; return; }
    n = (int)sys3(3, fd, (long)list, (long)sizeof(list) - 1);
    sys3(6, fd, 0, 0);
    if (n <= 0) { slog("UIL empty\n"); busy = 0; return; }
    list[n] = 0;
    for (i = 0; i < 5; i++) path[p++] = "view/"[i];
    for (i = 0; i < n && list[i] != '\n' && list[i] != '\r' && p < 90; i++)
        path[p++] = list[i];
    path[p] = 0;
    hexlog("UIL name ", (u32)p);
    { int r = g_ov(path, 0); hexlog("UIL ret  ", (u32)r); }
    busy = 0;
}
static int hook_rcv(u32 q, void *msg, int tmo)
{
    if (!tried) { tried = 1; try_open(); }
    return tramp_rcv ? tramp_rcv(q, msg, tmo) : -1;
}
static u32 build_tramp(u32 target, u32 nbytes, const unsigned char *orig)
{
    u32 page = (u32)sys6(192, 0, 0x1000, 7, 0x22, -1, 0);
    unsigned char *p; u32 off, i; volatile u32 *ins;
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
    void *h, *hl;
    int (*ov)(const char *, int);
    void *rcv;
    (void)data; (void)size; (void)arg2;
    slog("UIL enter\n");
    h = dlopen("/usr/lib/libObj.so", 1);
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    if (!ov) { slog("UIL missing ov\n"); return 0; }
    g_ov = ov;
    hl = dlopen("/usr/lib/libosal_uipc.so", 1);
    rcv = dlopen ? dlsym(hl, "osal_rcv_msg_tmo") : 0;
    hexlog("UIL rcv  ", (u32)rcv);
    if (rcv) {
        u32 t = (u32)rcv;
        volatile u32 *w = (volatile u32 *)t;
        hexlog("UIL w0   ", w[0]);
        hexlog("UIL w1   ", w[1]);
        {
            u32 tramp = build_tramp(t, 8, (const unsigned char *)t);
            hexlog("UIL tramp", tramp);
            if (tramp) {
                tramp_rcv = (int (*)(u32, void *, int))tramp;
                if (sys3(125, t & ~0xfffu, 0x1000, 7) == 0) {
                    volatile u32 *patch = (volatile u32 *)t;
                    patch[0] = 0xf000f8dfu;
                    patch[1] = (u32)hook_rcv;
                    cacheflush(t, t + 8);
                    sys3(125, t & ~0xfffu, 0x1000, 5);
                    slog("UIL inst\n");
                }
            }
        }
    }
    slog("UIL done\n");
    return 0;
}
