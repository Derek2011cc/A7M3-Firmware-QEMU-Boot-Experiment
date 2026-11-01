/* Scenario plugin: try opening the main menu views and log what loads.
 * Candidates from viewUnified2.so's registry: GLOBAL_MENU / SETTINGMENUX /
 * FNMENU.  Only used in QEMU. */
typedef unsigned int u32;
typedef int s32;
extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *handle, const char *name);
extern int dladdr(void *addr, void *info);
struct dlinfo { const char *fname; void *fbase; const char *sname; void *saddr; };
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
static void log_view_maps(void)
{
    static char buf[16384];
    int fd = (int)sys3(5, (long)"/proc/self/maps", 0, 0);
    int n = 0, i, s;
    if (fd < 0) return;
    n = (int)sys3(3, fd, (long)buf, sizeof(buf) - 1);
    sys1(6, fd);
    if (n <= 0) return;
    buf[n] = 0;
    for (i = 0; i < n; ) {
        int j = i, hit = 0, k;
        while (j < n && buf[j] != '\n') j++;
        for (k = i; k + 7 < j; k++) {
            if (buf[k]=='v'&&buf[k+1]=='i'&&buf[k+2]=='e'&&buf[k+3]=='w'&&buf[k+4]=='/') hit = 1;
        }
        if (hit) {
            char line[160]; int m = 0;
            for (s = i; s < j && m < 150; s++) line[m++] = buf[s];
            line[m++] = '\n';
            sys3(4, 2, (long)line, m);
        }
        i = j + 1;
    }
}

int scenario_run(u32 data, u32 size, u32 arg2)
{
    void *h;
    int (*ov)(const char *, int);
    (void)data; (void)size; (void)arg2;
    slog("SCN2 enter\n");
    h = dlopen("/usr/lib/libObj.so", 1);
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    if (!ov) { slog("SCN2 missing\n"); return 0; }
    hexlog("SCN2 ov=", (u32)ov);
    hexlog("SCN2 r_global=", (u32)ov("view/GLOBAL_MENU", 0));
    sleep_ms(1500);
    log_view_maps();
    hexlog("SCN2 r_settings=", (u32)ov("view/SETTINGMENUX", 0));
    sleep_ms(1500);
    log_view_maps();
    hexlog("SCN2 r_fn=", (u32)ov("view/FNMENU", 0));
    sleep_ms(1500);
    log_view_maps();
    slog("SCN2 done\n");
    return 0;
}
