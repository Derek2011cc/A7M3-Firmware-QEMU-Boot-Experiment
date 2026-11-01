/* Scenario plugin: resolve GPE-related addresses in im.elf's own maps. */
typedef unsigned int u32;
typedef int s32;
extern void *dlopen(const char *path, int mode);
extern void *dlsym(void *handle, const char *name);

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
static int hexstart(const char *p, const char *e, u32 *out_end)
{
    u32 v = 0;
    int n = 0;
    while (p < e && *p != '-') {
        char c = *p;
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else break;
        v = (v << 4) | (u32)d; p++; n++;
    }
    if (p >= e || *p != '-') return 0;
    p++;
    *out_end = 0;
    while (p < e && *p != ' ') {
        char c = *p;
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else return 0;
        *out_end = (*out_end << 4) | (u32)d; p++;
    }
    return 1;
}
static void find_addr(u32 addr, const char *tag)
{
    int i = 0;
    while (i < maps_len) {
        int j = i;
        while (j < maps_len && maps[j] != '\n') j++;
        if (j < maps_len) {
            u32 end;
            if (hexstart(maps + i, maps + j, &end)) {
                u32 start = 0; int k = i;
                while (k < j && maps[k] != '-') {
                    char c = maps[k]; int d;
                    if (c >= '0' && c <= '9') d = c - '0';
                    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                    else break;
                    start = (start << 4) | (u32)d; k++;
                }
                if (addr >= start && addr < end) {
                    char b[128]; int n = 0, m;
                    while (tag[n] && n < 12) { b[n] = tag[n]; n++; }
                    for (m = i; m < j && n < 120; m++) b[n++] = maps[m];
                    b[n++] = '\n';
                    sys3(4, 2, (long)b, n);
                }
            }
        }
        i = j + 1;
    }
}

int scenario_run(u32 data, u32 size, u32 arg2)
{
    void *h;
    int (*ov)(const char *, int);
    (void)data; (void)size; (void)arg2;
    slog("SCN enter\n");
    load_maps();
    find_addr(0x02053bb0, "MAP116B");
    find_addr(0x0814eec0, "MAP116W");
    find_addr(0x2053bb00, "MAP116U");
    find_addr(0x2053c270, "MAPKICK");
    find_addr(0x2053b280, "MAPDMA");
    find_addr(0x202e0200, "MAPDISP");
    find_addr(0x8154d780, "MAPRING");
    h = dlopen("/usr/lib/libObj.so", 1);
    ov = (int (*)(const char *, int))dlsym(h, "_ZN13viewManagerIf8openViewEPKci");
    if (!ov) { slog("SCN missing\n"); return 0; }
    (void)ov("view/BASE_MENU", 0);
    slog("SCN done\n");
    return 0;
}
