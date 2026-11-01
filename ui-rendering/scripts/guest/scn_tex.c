/* Scenario plugin: read texture VAs from the app and mirror to scanout PA. */
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
static u32 hx(const char *p, const char *e)
{
    u32 v = 0;
    while (p < e) {
        char c = *p++;
        u32 d;
        if (c >= '0' && c <= '9') d = (u32)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (u32)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (u32)(c - 'A' + 10);
        else break;
        v = (v << 4) | d;
    }
    return v;
}
/* Find mapping covering [addr, addr+len).  *perm gets perms chars. */
static int find_region(u32 addr, u32 len, char *perm, int permsz)
{
    int i = 0;
    while (i < maplen) {
        int j = i, k;
        while (j < maplen && mapsbuf[j] != '\n') j++;
        k = i;
        while (k < j && mapsbuf[k] != '-') k++;
        if (k < j) {
            u32 lo = hx(mapsbuf + i, mapsbuf + k);
            u32 hi = hx(mapsbuf + k + 1, mapsbuf + (k + 1 < j ? k + 9 : j));
            if (addr >= lo && addr + len <= hi) {
                int p = 0;
                k++;
                while (k < j && mapsbuf[k] != ' ') k++;
                while (k < j && mapsbuf[k] == ' ') k++;
                while (k < j && mapsbuf[k] != ' ' && p < permsz - 1) perm[p++] = mapsbuf[k++];
                perm[p] = 0;
                return 1;
            }
        }
        i = j + 1;
    }
    return 0;
}
static void log_map_line(u32 addr)
{
    int i = 0;
    while (i < maplen) {
        int j = i, k;
        while (j < maplen && mapsbuf[j] != '\n') j++;
        k = i;
        while (k < j && mapsbuf[k] != '-') k++;
        if (k < j) {
            u32 lo = hx(mapsbuf + i, mapsbuf + k);
            u32 hi = hx(mapsbuf + k + 1, mapsbuf + (k + 1 < j ? k + 9 : j));
            if (addr >= lo && addr < hi) {
                char b[200]; int n = 0, p;
                for (p = i; p < j && n < 190; p++) {
                    char c = mapsbuf[p];
                    if (c != 0 && c != '\r') b[n++] = c;
                }
                b[n++] = '\n';
                sys3(4, 2, (long)b, n);
                return;
            }
        }
        i = j + 1;
    }
    slog("NOMAPLINE\n");
}
static int mem_wr(u32 phys, const unsigned char *buf, u32 len)
{
    int fd = (int)sys3(5, (long)"/dev/mem", 2, 0);
    u32 done = 0;
    if (fd < 0) return -1;
    if (sys3(19, fd, (long)phys, 0) < 0) { sys3(6, fd, 0, 0); return -2; }
    while (done < len) {
        long n = sys3(4, fd, (long)(buf + done), len - done);
        if (n <= 0) { sys3(6, fd, 0, 0); return -3; }
        done += (u32)n;
    }
    sys3(6, fd, 0, 0);
    return 0;
}

static unsigned char texbuf[0x100000];
static volatile int done_tex;
static u32 app_base;

static void try_texture(void)
{
    char perm[8];
    u32 va = 0x0445c040u, bytes = 0x96000u;
    int ok;
    if (done_tex) return;
    load_maps();
    log_map_line(va);
    ok = find_region(va, bytes, perm, sizeof(perm));
    if (!ok) {
        ok = find_region(va, 16, perm, sizeof(perm));
        bytes = 16;
    }
    if (!ok || perm[0] != 'r') {
        u32 v[4];
        static int att;
        att++;
        v[0] = va; v[1] = ok; v[2] = ok ? *(u32 *)perm : 0; v[3] = (u32)att;
        if (att >= 5) {
            hexline("TEXNO", v, 4);
            done_tex = 2;
        }
        return;
    }
    {
        u32 i;
        const volatile unsigned char *p = (const volatile unsigned char *)va;
        for (i = 0; i < bytes; i++) texbuf[i] = p[i];
    }
    {
        u32 w[12];
        int i;
        w[0] = va; w[1] = bytes;
        for (i = 0; i < 10; i++) w[2 + i] = ((const u32 *)texbuf)[i];
        hexline("TEXVA", w, 12);
    }
    {
        int r = mem_wr(0x202e0200u, texbuf, bytes);
        u32 w[4];
        w[0] = (u32)r; w[1] = bytes; w[2] = ((const u32 *)texbuf)[0];
        w[3] = ((const u32 *)texbuf)[bytes / 4 - 1];
        hexline("TEXWR", w, 4);
    }
    done_tex = 1;
}

static int (*t_2d)(void *);
static int (*t_kick)(void *);
static volatile int n2d, nkick;
static u32 ret2d[6], retkick[6];

static int h_2d(void *p)
{
    n2d++;
    if (n2d <= 6) {
        u32 v[14]; int i;
        v[0] = (u32)p;
        for (i = 0; i < 8; i++) v[1 + i] = p ? ((const u32 *)p)[i] : 0;
        v[9] = (u32)__builtin_return_address(0);
        v[10] = (u32)n2d;
        v[11] = 0; v[12] = 0; v[13] = 0;
        hexline("T2D", v, 14);
        try_texture();
    }
    return t_2d ? t_2d(p) : 0;
}
static int h_kick(void *p)
{
    nkick++;
    if (nkick <= 6) {
        u32 v[8]; int i;
        v[0] = (u32)p;
        for (i = 0; i < 4; i++) v[1 + i] = p ? ((const u32 *)p)[i] : 0;
        v[5] = (u32)__builtin_return_address(0);
        v[6] = (u32)nkick; v[7] = 0;
        hexline("TKICK", v, 8);
    }
    return t_kick ? t_kick(p) : 0;
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
    if (which == 1) t_2d = (int (*)(void *))tramp;
    if (which == 2) t_kick = (int (*)(void *))tramp;
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
    app_base = base;
    if (base) {
        u32 t2 = base + 0x73a100u, tk = base + 0x73a290u;
        u32 w[8];
        t_2d = 0; t_kick = 0;
        w[0] = base; w[1] = t2; w[2] = *(volatile u32 *)t2; w[3] = tk;
        w[4] = *(volatile u32 *)tk; w[5] = (u32)h_2d; w[6] = (u32)h_kick; w[7] = 0;
        hexline("TINFO", w, 8);
        w[0] = install(t2, 8, (void *)h_2d, 0x41f0e92du, 1);
        w[1] = install(tk, 10, (void *)h_kick, 0x4604b5b0u, 2);
        w[2] = (u32)t_2d; w[3] = (u32)t_kick; w[4] = 0; w[5] = 0; w[6] = 0; w[7] = 0;
        hexline("TINST", w, 8);
    }
    load_maps();
    log_map_line(0x0445c040u);
    ov("view/BASE_MENU", 0);
    {
        u32 w[4];
        w[0] = (u32)n2d; w[1] = (u32)nkick; w[2] = (u32)done_tex; w[3] = 0;
        hexline("TDONE", w, 4);
    }
    slog("SCN done\n");
    return 0;
}
