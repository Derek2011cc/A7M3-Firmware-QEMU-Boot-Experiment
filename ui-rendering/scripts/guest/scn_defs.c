/* Scenario plugin: list loaded libraries that define GRM/GraphicsImage symbols. */
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
static char maps[65536];
static int maps_len;
static void load_maps(void)
{
    int fd = (int)sys3(5, (long)"/proc/self/maps", 0, 0);
    maps_len = 0;
    if (fd < 0) return;
    while (maps_len < (int)sizeof(maps) - 1) {
        long n = sys3(3, fd, (long)(maps + maps_len), sizeof(maps) - 1 - maps_len);
        if (n <= 0) break;
        maps_len += (int)n;
    }
    sys1(6, fd);
    maps[maps_len] = 0;
}
static const char *token5(const char *line, const char *end)
{
    int tok = 0;
    const char *p = line;
    while (p < end) {
        while (p < end && *p == ' ') p++;
        if (p >= end) break;
        if (tok == 5) return p;
        while (p < end && *p != ' ' && *p != '\n') p++;
        tok++;
    }
    return 0;
}
static void loglib(const char *path, const char *sym, void *addr)
{
    char b[160]; int n = 0, i;
    struct dlinfo di;
    const char *def = path;
    if (dladdr(addr, &di) && di.fname) def = di.fname;
    while (sym[n] && n < 24) { b[n] = sym[n]; n++; }
    b[n++] = ' ';
    for (i = 0; def[i] && n < 120; i++) b[n++] = def[i];
    b[n++] = ' ';
    {
        static const char d[] = "0123456789abcdef";
        int j;
        for (j = 28; j >= 0; j -= 4) b[n++] = d[((u32)(unsigned long)addr >> j) & 15];
    }
    b[n++] = '\n';
    sys3(4, 2, (long)b, n);
}

int scenario_run(u32 data, u32 size, u32 arg2)
{
    void *h;
    int (*ov)(const char *, int);
    (void)data; (void)size; (void)arg2;
    slog("SCN enter\n");
    load_maps();
    {
        int i = 0;
        while (i < maps_len) {
            int j = i;
            while (j < maps_len && maps[j] != '\n') j++;
            {
                const char *p = token5(maps + i, maps + j);
                if (p && *p == '/') {
                    char path[160]; int k = 0;
                    while (p + k < maps + j && k < 150) { path[k] = p[k]; k++; }
                    path[k] = 0;
                    if (k > 3) {
                        void *lh = dlopen(path, 5);
                        if (lh) {
                            void *a1 = dlsym(lh, "GRM_gpermRectblit");
                            void *a2 = dlsym(lh, "GRM_gpermFillrect");
                            void *a3 = dlsym(lh, "_ZN2ux4core13GraphicsImage5clearEPvRKN6stella6CColorEssss");
                            void *a4 = dlsym(lh, "_ZN27C_DataflowInfra_StdData_OSD25SetEntry_OsdImageSizeInfoEP34C_DataflowInfra_Entry_OsdImageSize");
                            if (a1) loglib(path, "RECT", a1);
                            if (a2) loglib(path, "FILL", a2);
                            if (a3) loglib(path, "GICLEAR", a3);
                            if (a4) loglib(path, "OSDSET", a4);
                        }
                    }
                }
            }
            i = j + 1;
        }
    }
    h = dlopen("/usr/lib/libObj.so", 1);
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    slog("SCN done\n");
    return 0;
}
