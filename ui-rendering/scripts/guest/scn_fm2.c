/* Like scn_fm.c but preloads dependencies with RTLD_GLOBAL and logs dlerror. */
typedef unsigned int u32;
typedef int s32;
extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *handle, const char *name);
extern char *dlerror(void);
struct timespec { long tv_sec; long tv_nsec; };

static long sys3(long n, long a, long b, long c)
{
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r7 __asm__("r7") = n;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
static void logstr(const char *t)
{
    int n = 0;
    if (!t) { sys3(4, 2, (long)"(null)\n", 7); return; }
    while (t[n]) n++;
    sys3(4, 2, (long)t, n);
    sys3(4, 2, (long)"\n", 1);
}
static void slog(const char *t) { int n=0; while(t[n]) n++; sys3(4,2,(long)t,n); }
static void hexlog(const char *tag, u32 v)
{
    static const char d[] = "0123456789abcdef";
    char b[64]; int n = 0, i;
    while (tag[n] && n < 28) { b[n] = tag[n]; n++; }
    for (i = 28; i >= 0; i -= 4) b[n++] = d[(v >> i) & 15];
    b[n++] = '\n';
    sys3(4, 2, (long)b, n);
}
static void sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    sys3(162, (long)&ts, 0, 0);
}

static void try_module(const char *path, const char *tag)
{
    void *h;
    int (*run)(u32, u32, u32);
    static u32 payload[2];
    static const u32 variants[4][2] = {{0,0},{1,0},{0,1},{1,1}};
    int i;

    h = dlopen(path, 1);
    hexlog(tag, (u32)h);
    if (!h) { logstr(dlerror()); return; }
    run = (int (*)(u32, u32, u32))dlsym(h, "scenario_run");
    hexlog("RUN ", (u32)run);
    if (!run) return;
    for (i = 0; i < 4; i++) {
        payload[0] = variants[i][0];
        payload[1] = variants[i][1];
        { int r = run((u32)payload, 8, 4); hexlog("RET ", (u32)r); }
        sleep_ms(1000);
    }
}

int scenario_run(u32 data, u32 size, u32 arg2)
{
    (void)data; (void)size; (void)arg2;
    slog("FM2 enter\n");
    hexlog("mpr=", (u32)dlopen("/usr/lib/libmpr.so", 0x101));
    hexlog("mwf=", (u32)dlopen("/usr/lib/libMWF.so", 0x101));
    hexlog("osal=", (u32)dlopen("/usr/lib/libosal_uipc.so", 0x101));
    try_module("/usr/scenario/MPR_SCN_SET_FACTORY_MODE.so", "HSET");
    try_module("/usr/scenario/MPR_SCN_EXEC_FACTORY_MODE.so", "HEXE");
    slog("FM2 done\n");
    return 0;
}
