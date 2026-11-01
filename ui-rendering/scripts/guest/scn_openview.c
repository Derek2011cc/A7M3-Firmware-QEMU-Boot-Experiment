/* Scenario plugin: identify process, libObj base, and open BASE_MENU. */
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
static long sys1(long n, long a)
{
    register long r0 __asm__("r0") = a;
    register long r7 __asm__("r7") = n;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r7) : "memory");
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
    char b[48]; int n = 0, i;
    while (tag[n] && n < 24) { b[n] = tag[n]; n++; }
    for (i = 28; i >= 0; i -= 4) b[n++] = d[(v >> i) & 15];
    b[n++] = '\n';
    sys3(4, 2, (long)b, n);
}
static void logtext(const char *tag, const char *s, int max)
{
    char b[96]; int n = 0, i;
    while (tag[n] && n < 16) { b[n] = tag[n]; n++; }
    for (i = 0; i < max && s[i]; i++) b[n + i] = s[i] ? s[i] : '.';
    n += i;
    b[n++] = '\n';
    sys3(4, 2, (long)b, n);
}

int scenario_run(u32 data, u32 size, u32 arg2)
{
    void *h;
    int (*ov)(const char *, int);
    int r;
    u32 base = 0;
    (void)data; (void)size; (void)arg2;
    slog("SCN enter\n");
    hexlog("SCN pid=", (u32)sys1(20, 0));
    {
        char cb[64];
        int fd = (int)sys3(5, (long)"/proc/self/cmdline", 0, 0);
        int n = 0;
        if (fd >= 0) { n = (int)sys3(3, fd, (long)cb, 64); sys1(6, fd); }
        if (n > 0) { volatile char *p = cb; for (int i = 0; i < n; i++) if (!p[i]) p[i] = '.'; cb[n-1] = 0; logtext("SCN cmd=", cb, 60); }
    }
    h = dlopen("/usr/lib/libObj.so", 1);
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    if (!ov) { slog("SCN missing\n"); return 0; }
    hexlog("SCN ov=", (u32)ov);
    {
        struct dlinfo di;
        if (dladdr((void *)((u32)ov & ~1u), &di)) {
            base = (u32)di.fbase;
            hexlog("SCN base=", base);
            if (di.fname) logtext("SCN ovlib=", di.fname, 48);
        }
    }
    r = ov("view/BASE_MENU", 0);
    hexlog("SCN r1=", (u32)r);
    slog("SCN done\n");
    return 0;
}
