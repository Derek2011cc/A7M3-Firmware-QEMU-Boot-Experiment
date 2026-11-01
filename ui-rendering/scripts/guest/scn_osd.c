/* Scenario plugin: patch OSD dataflow GOT slots and log entries. */
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
static int (*real_setentry)(u32, u32);
static int (*real_send)(u32, u32, u32, u32);
static volatile int nset, nsend;

static u32 safe(u32 p) { return (p > 0x1000 && p < 0x60000000) ? p : 0; }

static int hook_setentry(u32 self, u32 entry)
{
    if (nset < 64) {
        u32 v[18]; int i;
        v[0] = self; v[1] = entry;
        for (i = 0; i < 16; i++) v[2 + i] = safe(entry) ? ((const u32 *)entry)[i] : 0;
        hexline("OSDSET", v, 18);
        nset++;
    }
    return real_setentry ? real_setentry(self, entry) : 0;
}
static int hook_send(u32 pin, u32 a1, u32 a2, u32 a3)
{
    if (nsend < 64) {
        u32 v[18]; int i;
        v[0] = pin; v[1] = a1; v[2] = a2; v[3] = a3;
        for (i = 0; i < 14; i++) v[4 + i] = safe(pin) ? ((const u32 *)pin)[i] : 0;
        hexline("OSDSND", v, 18);
        nsend++;
    }
    return real_send ? real_send(pin, a1, a2, a3) : 0;
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
    if (base) {
        volatile u32 *s1 = (volatile u32 *)(base + 0x126af44u);
        volatile u32 *s2 = (volatile u32 *)(base + 0x126b420u);
        u32 old1 = *s1, old2 = *s2;
        void *rt1 = dlsym(0, "_ZN27C_DataflowInfra_StdData_OSD25SetEntry_OsdImageSizeInfoEP34C_DataflowInfra_Entry_OsdImageSize");
        void *rt2 = dlsym(0, "_ZN16DataflowInfraPin4SendEmtm");
        u32 v[8];
        real_setentry = (int (*)(u32, u32))rt1;
        real_send = (int (*)(u32, u32, u32, u32))rt2;
        if (!real_setentry && old1 > 0x1000) real_setentry = (int (*)(u32, u32))old1;
        if (!real_send && old2 > 0x1000) real_send = (int (*)(u32, u32, u32, u32))old2;
        v[0] = base; v[1] = old1; v[2] = old2; v[3] = (u32)real_setentry;
        v[4] = (u32)real_send; v[5] = (u32)hook_setentry; v[6] = (u32)hook_send; v[7] = 0;
        hexline("OSDINFO", v, 8);
        {
            u32 m1 = (u32)sys3(125, (base + 0x126a000u), 0x1000, 3);
            u32 m2 = (u32)sys3(125, (base + 0x126b000u), 0x1000, 3);
            u32 w[6];
            w[0] = m1; w[1] = m2;
            *s1 = (u32)hook_setentry;
            *s2 = (u32)hook_send;
            w[2] = *s1; w[3] = *s2; w[4] = (u32)ov; w[5] = 0;
            hexline("OSDPATCH", w, 6);
        }
    }
    ov("view/BASE_MENU", 0);
    {
        u32 w[2];
        w[0] = (u32)nset; w[1] = (u32)nsend;
        hexline("OSDDONE", w, 2);
    }
    slog("SCN done\n");
    return 0;
}
