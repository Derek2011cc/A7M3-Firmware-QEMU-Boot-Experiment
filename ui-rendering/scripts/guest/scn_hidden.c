/* Scenario plugin: open a list of hidden views read from /tmp/viewlist.
 * Each line is a view name without the "view/" prefix.  Used in QEMU only. */
typedef unsigned int u32;
typedef int s32;
extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *handle, const char *name);
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
static void wlog(const char *t, int n)
{
    sys3(4, 2, (long)t, n);
}
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void slog(const char *t) { wlog(t, slen(t)); }
static void hexlog(const char *tag, u32 v)
{
    static const char d[] = "0123456789abcdef";
    char b[64]; int n = 0, i;
    while (tag[n] && n < 24) { b[n] = tag[n]; n++; }
    for (i = 28; i >= 0; i -= 4) b[n++] = d[(v >> i) & 15];
    b[n++] = '\n';
    wlog(b, n);
}
static void sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    sys3(162, (long)&ts, 0, 0);
}

int scenario_run(u32 data, u32 size, u32 arg2)
{
    void *h;
    int (*ov)(const char *, int);
    char list[1024];
    int fd, n, i, start;
    (void)data; (void)size; (void)arg2;

    slog("SCNH enter\n");
    h = dlopen("/usr/lib/libObj.so", 1);
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    if (!ov) { slog("SCNH missing\n"); return 0; }
    hexlog("SCNH ov=", (u32)ov);

    fd = (int)sys3(5, (long)"/tmp/viewlist", 0, 0);
    if (fd < 0) { slog("SCNH no list\n"); return 0; }
    n = (int)sys3(3, fd, (long)list, (long)sizeof(list) - 1);
    sys1(6, fd);
    if (n <= 0) { slog("SCNH empty\n"); return 0; }
    list[n] = 0;

    start = 0;
    for (i = 0; i <= n; i++) {
        if (i == n || list[i] == '\n' || list[i] == '\r') {
            int j = start, k = start;
            char path[96];
            int p = 0;
            if (i > start) {
                while (k < i && (list[k] == ' ' || list[k] == '\t')) k++;
                while (j < i && list[j] != ' ') j++;
                for (p = 0; p < 5; p++) path[p] = "view/"[p];
                for (j = k; j < i && p < 90; j++) path[p++] = list[j];
                path[p] = 0;
                hexlog("SCNH open ", (u32)p);
                {
                    int r = ov(path, 0);
                    hexlog("SCNH ret  ", (u32)r);
                }
                sleep_ms(1500);
            }
            start = i + 1;
        }
    }
    slog("SCNH done\n");
    return 0;
}
