/* Global UIPC observer for the A7M3 QEMU experiment.
 * Resolves the real libosal_uipc functions from /proc/self/maps and known
 * symbol offsets, so it needs neither libc nor libdl. */
typedef unsigned int u32;
typedef int s32;

#define NR_READ  3
#define NR_WRITE 4
#define NR_OPEN  5
#define NR_CLOSE 6
#define NR_EXECVE 11
#define NR_IOCTL 54
#define NR_GETTID 224

static long sc3(long n, long a, long b, long c)
{
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r7 __asm__("r7") = n;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory");
    return r0;
}
static long sc4(long n, long a, long b, long c, long d)
{
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r3 __asm__("r3") = d;
    register long r7 __asm__("r7") = n;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3), "r"(r7) : "memory");
    return r0;
}
static int tid(void) { return (int)sc3(NR_GETTID, 0, 0, 0); }

static char lb[340]; static int ln;
static void s(const char *p){ while(*p && ln<320) lb[ln++]=*p++; }
static void h(u32 v){ static const char d[]="0123456789abcdef"; int i; for(i=28;i>=0;i-=4) if(ln<320) lb[ln++]=d[(v>>i)&15]; }
static void d(s32 v){ char t[12]; int n=0; if(v<0){ if(ln<320) lb[ln++]='-'; v=-v;} if(!v){ if(ln<320) lb[ln++]='0'; return;} while(v&&n<12){ t[n++]='0'+v%10; v/=10;} while(n) if(ln<320) lb[ln++]=t[--n]; }
static void f(void){ if(ln>0){ sc3(NR_WRITE,2,(long)lb,ln); ln=0; } }
static void beg(const char *tag){ ln=0; s("[U t"); d(tid()); s("] "); s(tag); }

/* Safe payload dump: only read if the address is in a readable mapping of
 * this process, so a hostile/bogus message word can never fault the app. */
static void dump_msg_payload(u32 a, u32 len);
static int addr_readable(u32 a, u32 len);

static int str_in(const char *s, const char *sub);

/* Observe module launches: bootin forks and calls execve.
 * For im.elf, append "target=qemu" to select the firmware's own qemu mode. */
s32 execve(const char *path, char *const argv[], char *const envp[])
{
    static char *qa[24];
    const char *const *av = argv;
    int i, n = 0;
    beg("EXEC ");
    s(path ? path : "(null)");
    if (argv) {
        for (i = 0; i < 8 && argv[i]; i++) { s(" ["); s(argv[i]); s("]"); }
    }
    if (path && argv && str_in(path, "im.elf")) {
        while (argv[n] && n < 20) { qa[n] = argv[n]; n++; }
        qa[n++] = "target=qemu";
        qa[n] = 0;
        av = qa;
        s(" +target=qemu");
    }
    f();
    {
        long r = sc3(NR_EXECVE, (long)path, (long)av, (long)envp);
        beg("EXECFAIL r="); d((s32)r); f();
        return (s32)r;
    }
}

static int str_in(const char *s, const char *sub)
{
    if (!s) return 0;
    while (*s) { const char *a=s,*b=sub; while (*a && *b && *a==*b){a++;b++;} if(!*b) return 1; s++; }
    return 0;
}
static void log_open(const char *fn, const char *path, s32 flags)
{
    if (path && (str_in(path, "/dev/") || str_in(path, "fb") || str_in(path, "sugi") || str_in(path, "grm"))) {
        beg("OPEN "); s(fn); s(" "); s(path); s(" fl="); h((u32)flags); f();
    }
}
static int ldec_fd = -1;
s32 open(const char *path, s32 flags, s32 mode)
{
    s32 r;
    log_open("open", path, flags);
    r = (s32)sc3(NR_OPEN, (long)path, flags, mode);
    if (r >= 0 && path && str_in(path, "/dev/ldec")) ldec_fd = r;
    return r;
}
#define NR_OPENAT 322
s32 openat(s32 dirfd, const char *path, s32 flags, s32 mode)
{
    s32 r;
    log_open("openat", path, flags);
    r = (s32)sc4(NR_OPENAT, dirfd, (long)path, flags, mode);
    if (r >= 0 && path && str_in(path, "/dev/ldec")) ldec_fd = r;
    return r;
}
s32 open64(const char *path, s32 flags, s32 mode) { return open(path, flags, mode); }
s32 openat64(s32 dirfd, const char *path, s32 flags, s32 mode) { return openat(dirfd, path, flags, mode); }

/* Log Sugilite ioctls (0xc00482xx / 0x800482xx) to identify the wait command
 * and its user argument.  Other ioctls pass straight through. */
/* ---- Sony LZ77 software decode for /dev/ldec emulation ----
 * Decoder adapted from fwtool (ma1co), MIT. ldec ioctl arg (seen at runtime):
 *   [0]=src user VA, [1]=srclen, [2]=dst physical, [3]=dstlen, [4]=2, [5]=3,
 *   [6]=0, [7]=0x90.  The hardware LZ77 DMA is not modeled, so decode on the
 *   AP and write the result to the physical destination before returning. */
static unsigned char lz_in[0x110000];
static unsigned char lz_out[0x110000];
static int mem_fd = -1;

static int mem_open(void)
{
    if (mem_fd < 0) mem_fd = (int)sc3(NR_OPEN, (long)"/dev/mem", 2, 0);
    return mem_fd;
}
static int mem_rd(u32 phys, unsigned char *buf, u32 len)
{
    int fd = mem_open(); u32 done = 0;
    if (fd < 0) return -1;
    if (sc3(19, fd, (long)phys, 0) < 0) return -1;
    while (done < len) {
        long n = sc3(NR_READ, fd, (long)(buf + done), len - done);
        if (n <= 0) return -1;
        done += (u32)n;
    }
    return 0;
}
static int mem_wr(u32 phys, const unsigned char *buf, u32 len)
{
    int fd = mem_open(); u32 done = 0;
    if (fd < 0) return -1;
    if (sc3(19, fd, (long)phys, 0) < 0) return -1;
    while (done < len) {
        long n = sc3(NR_WRITE, fd, (long)(buf + done), len - done);
        if (n <= 0) return -1;
        done += (u32)n;
    }
    return 0;
}
static int lz_walk(const unsigned char *src, u32 size, unsigned char *dst, u32 cap)
{
    static const unsigned char lengths[16] = {3,4,5,6,7,8,9,10,11,12,13,14,15,16,32,64};
    u32 in = 0, out = 0, i;
    while (in < size) {
        unsigned int type = src[in++];
        u32 block_start = out;
        if (type == 0x0f) {
            u32 count;
            if (size - in < 3) return 5;
            ++in;
            count = (u32)src[in] | ((u32)src[in + 1] << 8);
            in += 2;
            if (count > size - in || count > cap - out) return 5;
            if (dst) for (i = 0; i < count; ++i) dst[out + i] = src[in + i];
            in += count; out += count;
        } else if (type == 0xf0) {
            int finished = 0;
            while (!finished) {
                unsigned int flags, bit;
                if (in == size) return 5;
                flags = src[in++];
                for (bit = 0; bit < 8; ++bit) {
                    if (flags & (1u << bit)) {
                        unsigned int first, second, distance, count;
                        if (size - in < 2) return 5;
                        first = src[in++]; second = src[in++];
                        distance = ((first & 15) << 8) | second;
                        if (!distance) { finished = 1; break; }
                        count = lengths[first >> 4];
                        if (distance > out - block_start || count > cap - out) return 5;
                        if (dst) for (i = 0; i < count; ++i) dst[out + i] = dst[out + i - distance];
                        out += count;
                    } else {
                        if (in == size || out == cap) return 5;
                        if (dst) dst[out] = src[in];
                        ++in; ++out;
                    }
                }
            }
        } else return 5;
    }
    return out == cap ? 0 : 5;
}
static int lz_decode(const unsigned char *src, u32 size, unsigned char *dst, u32 cap)
{
    if (!src || !size || size > 0x100000u || !cap || cap > 0x100000u) return 5;
    if (lz_walk(src, size, 0, cap)) return 5;
    return lz_walk(src, size, dst, cap);
}
static int ldec_try(u32 arg)
{
    u32 *m = (u32 *)arg;
    u32 src, slen, dst, dlen, i;
    int ok;
    if (!addr_readable(arg, 32)) return 0;
    src = m[0]; slen = m[1]; dst = m[2]; dlen = m[3];
    if (!src || !slen || !dst || !dlen || slen > 0x100000u || dlen > 0x100000u) return 0;
    if (!addr_readable(src, slen)) return 0;
    for (i = 0; i < slen; i++) lz_in[i] = ((const unsigned char *)src)[i];
    if (lz_decode(lz_in, slen, lz_out, dlen) != 0) return -1;
    if (addr_readable(dst, dlen)) {
        for (i = 0; i < dlen; i++) ((unsigned char *)dst)[i] = lz_out[i];
        ok = 0;
    } else {
        ok = mem_wr(dst, lz_out, dlen);
    }
    return ok == 0 ? 1 : -1;
}

s32 ioctl(s32 fd, u32 req, u32 arg)
{
    long r;
    if ((req >> 16) == 0xc004u || (req >> 16) == 0x8004u) {
        beg("IOCTL fd="); d(fd); s(" cmd="); h(req); s(" arg="); h(arg);
        if (arg >= 0x1000u && arg < 0x80000000u) { s(" v="); h(*(volatile u32 *)arg); }
        f();
    }
    if (fd == ldec_fd) {
        beg("LDEC> cmd="); h(req); s(" arg="); h(arg);
        if (arg > 0x1000u && arg < 0x80000000u && addr_readable(arg, 32)) {
            u32 *m = (u32 *)arg; int i;
            s(" b=");
            for (i = 0; i < 8; i++) { s(" "); h(m[i]); }
            if (m[2] > 0x1000u && m[2] < 0x80000000u && addr_readable(m[2], 16)) {
                u32 *sp = (u32 *)m[2];
                s(" src="); h(m[2]); s(" len="); h(m[3]); s(" d=");
                for (i = 0; i < 4; i++) { s(" "); h(sp[i]); }
            }
        }
        f();
        if (req == 1u && arg > 0x1000u && arg < 0x80000000u) {
            int lr = ldec_try(arg);
            if (lr == 1) {
                u32 *m = (u32 *)arg;
                beg("LDEC OK sl="); h(m[1]); s(" dl="); h(m[3]); f();
                return 0;
            }
            beg("LDEC skip lr="); d(lr); f();
        }
    }
    r = sc3(NR_IOCTL, fd, req, arg);
    if ((req >> 16) == 0xc004u || (req >> 16) == 0x8004u) {
        beg("IOCTL< cmd="); h(req); s(" ret="); d((s32)r);
        if (arg >= 0x1000u && arg < 0x80000000u) { s(" v="); h(*(volatile u32 *)arg); }
        f();
    }
    if (fd == ldec_fd) { beg("LDEC< ret="); d((s32)r); f(); }
    return (s32)r;
}

static int interesting(u32 qid)
{
    u32 hi = qid >> 16, lo = qid & 0xffff;
    if (hi == 0x0091u || hi == 0x0092u || hi == 0x0094u ||
        hi == 0x008fu || hi == 0x003eu) return 1;
    if (qid == 0x5503e0u || qid == 0x807a0020u || qid == 0x007a804cu ||
        qid == 0x017060a1u || qid == 0x0037807du) return 1;
    if (qid == 0x00dc0292u || qid == 0x00dc0293u || qid == 0x00dc0294u) return 1;
    if (qid == 0x007a0020u || qid == 0x00410025u || qid == 0x008f0313u) return 1;
    if (qid == 0x007a0027u || qid == 0x007a0028u || qid == 0x007a0029u) return 1;
    if (lo >= 0x6000u) return 1;
    return 0;
}

/* Low-bandwidth watch set: periodic polls and ulog streams are excluded so
 * the 115200-baud console does not starve shell input. */
static int verbose_qid(u32 qid)
{
    if (qid == 0x0094016au || qid == 0x007a0020u || qid == 0x007a804cu ||
        qid == 0x00dc0292u || qid == 0x00dc0293u || qid == 0x00924126u ||
        qid == 0x00924127u || qid == 0x007a0027u || qid == 0x007a0028u ||
        qid == 0x007a0029u || qid == 0x00410025u) return 1;
    return 0;
}

static u32 hexval(const char *p, int n)
{
    u32 v = 0; int i;
    for (i = 0; i < n; i++) {
        char c = p[i]; u32 x;
        if (c >= '0' && c <= '9') x = c - '0';
        else if (c >= 'a' && c <= 'f') x = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') x = c - 'A' + 10;
        else break;
        v = (v << 4) | x;
    }
    return v;
}

struct link_map32 {
    u32 l_addr;
    char *l_name;
    void *l_ld;
    struct link_map32 *l_next;
    struct link_map32 *l_prev;
};
extern char _r_debug[] __attribute__((weak));

static int name_has(const char *s, const char *sub)
{
    while (*s) {
        const char *a = s, *b = sub;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b) return 1;
        s++;
    }
    return 0;
}

static u32 find_lib(const char *name)
{
    struct link_map32 *lm;
    if (!_r_debug) return 0;
    lm = *(struct link_map32 **)(_r_debug + 4);
    while (lm) {
        if (lm->l_name && name_has(lm->l_name, name)) return lm->l_addr;
        lm = lm->l_next;
    }
    return 0;
}

static const char *addr_lib2(u32 a, u32 *base)
{
    struct link_map32 *lm;
    const char *best = 0;
    u32 bb = 0;
    if (!_r_debug) return 0;
    lm = *(struct link_map32 **)(_r_debug + 4);
    while (lm) {
        if (lm->l_addr && lm->l_addr <= a && lm->l_addr >= bb) { bb = lm->l_addr; best = lm->l_name; }
        lm = lm->l_next;
    }
    if (base) *base = bb;
    return best;
}

static u32 osal_base(void)
{
    static u32 base; static int done;
    if (done) return base;
    done = 1;
    base = find_lib("libosal_uipc.so");
    if (!base) { beg("LMAP no"); f(); }
    return base;
}

typedef s32 (*rcv_fn)(u32, void **, s32);
typedef s32 (*snd_fn)(u32, void *);
typedef s32 (*valloc_fn)(u32, void **, u32, u32);
typedef s32 (*reg_fn)(u32, u32, u32, u32);
typedef s32 (*free_fn)(u32, void *);
static rcv_fn r_rcv; static snd_fn r_snd, r_ssnd;
static valloc_fn r_valloc; static reg_fn r_reg, r_regs, r_dyn;
static free_fn r_free;
static void ah_install(void);

#define OFF_RCV   0x3ed9u
#define OFF_SND   0x3c35u
#define OFF_SSND  0x3ec9u
#define OFF_VAL   0x3fedu
#define OFF_REG   0x4301u
#define OFF_REGS  0x3ae1u
#define OFF_DYN   0x4841u
#define OFF_FREE  0x4089u

static void nodl(const char *fn, u32 q)
{
    beg("NODL "); s(fn); s(" q="); h(q); f();
}

static void log_words(u32 *m, int n)
{
    int i; for (i=0;i<n;i++){ s(" "); h(m[i]); }
}

/* Ask bootin to start the GUI module (/usr/bin/im.elf g) once, at the moment
 * it enters its receive loop. Type 0x00940032 is the "g" entry of bootin's
 * command table (see bootin.elf 0x9d44). */
static void launch_gui(u32 qid)
{
    void *m = 0;
    if (!r_valloc) { u32 b = osal_base(); if (b) r_valloc = (valloc_fn)(b + OFF_VAL); }
    if (!r_snd) { u32 b = osal_base(); if (b) r_snd = (snd_fn)(b + OFF_SND); }
    if (!r_valloc || !r_snd) { beg("LAUNCH nodl"); f(); return; }
    if (r_valloc(qid, &m, 0x10, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = qid; p[0] = 0x00940032u; p[1] = 0; p[2] = 0; p[3] = 0;
        r_snd(qid, m);
        beg("LAUNCH gui q="); h(qid); f();
    } else {
        beg("LAUNCH allocfail q="); h(qid); f();
    }
}

static u32 gui_qid; static int gui_stage;
/* The UI thread is identified by the thread that pushes view events. */
static int ui_tid_seen; static int ui_tid; static int ov_tried;

static void send_type_to(u32 qid, u32 type)
{
    void *m = 0;
    if (!r_valloc) { u32 b = osal_base(); if (b) r_valloc = (valloc_fn)(b + OFF_VAL); }
    if (!r_snd) { u32 b = osal_base(); if (b) r_snd = (snd_fn)(b + OFF_SND); }
    if (!r_valloc || !r_snd) return;
    if (r_valloc(qid, &m, 0x10, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = qid; p[0] = type; p[1] = 0; p[2] = 0; p[3] = 0;
        r_snd(qid, m);
        beg("SENT "); h(type); s(" to "); h(qid); f();
    }
}

s32 osal_rcv_msg_tmo(u32 qid, void **msg, s32 timeout)
{
    s32 ret;
    u32 ra = (u32)__builtin_return_address(0);
    static int gui_launched;
    if (!r_rcv) { u32 b = osal_base(); if (b) r_rcv = (rcv_fn)(b + OFF_RCV); }
    if (!r_rcv) { nodl("rcv", qid); return -1; }
    ah_install();
    if (0 && qid == 0x0094020au && !gui_launched) { gui_launched = 1; launch_gui(qid); }
    if (gui_qid && qid == gui_qid) {
        if (gui_stage == 0) { send_type_to(qid, 0x00940007u); gui_stage = 9; }
    }
    if (ui_tid_seen && !ov_tried && tid() == ui_tid) {
        u32 bb = find_lib("libObj.so");
        if (bb) {
            u32 pvm = *(volatile u32 *)(bb + 0x13289f4u);
            if (pvm) {
                s32 (*ov)(u32,u32,s32) = (void *)(bb + 0x1349ddu);
                ov_tried = 1;
                beg("OVCALL2 pvm="); h(pvm); s(" q="); h(qid); f();
                { s32 r = ov(pvm, (u32)"view/BASE_MENU", 0); beg("OVRET2 r="); d(r); f(); }
            }
        }
    }
    if (verbose_qid(qid)) { beg("RCV q="); h(qid); s(" to="); d(timeout); s(" ra="); h(ra); { const char *ln = addr_lib2(ra, 0); if (ln) { s(" lib="); s(ln); } } f(); }
    ret = r_rcv(qid, msg, timeout);
    if (ret == 0 && msg && *msg && verbose_qid(qid)) {
        u32 *m = (u32 *)*msg;
        beg("RCVOK q="); h(qid); s(" ty="); h(m[0]); s(" src="); h(m[-1]); s(" b="); log_words(m, 4);
        if (qid == 0x00dc0292u || qid == 0x00dc0293u) dump_msg_payload((u32)*msg, 32);
        f();
    }
    return ret;
}

s32 osal_snd_msg(u32 qid, void *msg)
{
    u32 ra = (u32)__builtin_return_address(0);
    if (!r_snd) { u32 b = osal_base(); if (b) r_snd = (snd_fn)(b + OFF_SND); }
    if (!r_snd) { nodl("snd", qid); return -1; }
    ah_install();
    if (msg && (qid == 0x005503e0u || qid == 0x807a0020u)) {
        u32 *m = (u32 *)msg; s32 ret;
        beg("SNDX q="); h(qid); s(" ty="); h(m[0]); s(" w="); log_words(m, 8); f();
        ret = r_snd(qid, msg);
        beg("SNDX< q="); h(qid); s(" ret="); d(ret); f();
        return ret;
    }
    if (msg && (qid == 0x008f013au || qid == 0x008f0139u))
        return r_snd(qid, msg);
    if (msg && (qid == 0x00dc0292u || qid == 0x00dc0293u)) {
        u32 *m = (u32 *)msg; s32 ret;
        beg("TCMD> q="); h(qid); s(" src="); h(m[-1]); s(" b="); log_words(m, 12);
        dump_msg_payload((u32)msg, 32);
        f();
        ret = r_snd(qid, msg);
        beg("TCMD< q="); h(qid); s(" ret="); d(ret); f();
        return ret;
    }
    if (msg && verbose_qid(qid)) {
        u32 *m = (u32 *)msg;
        beg("SND q="); h(qid); s(" ty="); h(m[0]); s(" src="); h(m[-1]); s(" ra="); h(ra); s(" b="); log_words(m, 4); f();
    }
    return r_snd(qid, msg);
}

s32 osal_snd_sync_msg(u32 qid, void *msg)
{
    u32 ra = (u32)__builtin_return_address(0);
    s32 ret;
    if (!r_ssnd) { u32 b = osal_base(); if (b) r_ssnd = (snd_fn)(b + OFF_SSND); }
    if (!r_ssnd) { nodl("ssnd", qid); return -1; }
    if (msg && (qid == 0x008f013au || qid == 0x008f0139u))
        return r_ssnd(qid, msg);
    if (msg && (qid == 0x00924127u || qid == 0x00924126u || qid == 0x0094016au)) {
        u32 *m = (u32 *)msg;
        beg("SSND> q="); h(qid); s(" ty="); h(m[0]); s(" b="); log_words(m, 4);
        if (m[0] == 0x00940021u && m[3] > 0x1000u && m[3] < 0x80000000u)
            dump_msg_payload(m[3], 64);
        f();
        ret = r_ssnd(qid, msg);
        beg("SSND< q="); h(qid); s(" ret="); d(ret); s(" w="); log_words(m, 8); f();
        return ret;
    }
    if (msg && verbose_qid(qid)) {
        u32 *m = (u32 *)msg;
        beg("SSND q="); h(qid); s(" ty="); h(m[0]); s(" ra="); h(ra); s(" b="); log_words(m, 4); f();
    }
    return r_ssnd(qid, msg);
}

s32 osal_free_msg(u32 qid, void *msg)
{
    if (!r_free) { u32 b = osal_base(); if (b) r_free = (free_fn)(b + OFF_FREE); }
    if (!r_free) { nodl("free", qid); return -1; }
    if (msg && verbose_qid(qid)) {
        u32 *m = (u32 *)msg;
        beg("FREE q="); h(qid); s(" ty="); h(m[0]); s(" src="); h(m[-1]); f();
    }
    return r_free(qid, msg);
}

s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags)
{
    s32 ret;
    if (!r_valloc) { u32 b = osal_base(); if (b) r_valloc = (valloc_fn)(b + OFF_VAL); }
    if (!r_valloc) { nodl("val", qid); return -1; }
    ret = r_valloc(qid, msg, size, flags);
    if (ret == 0 && verbose_qid(qid)) { beg("VAL q="); h(qid); s(" sz="); d((s32)size); f(); }
    return ret;
}

/* Wrap callbacks registered on interesting queues so we can see the exact
 * messages the modules receive (especially UPM notifications). */
typedef s32 (*cb4_fn)(u32, u32, u32, u32);
#define CB_SLOTS 12
static cb4_fn orig_cb[CB_SLOTS];
static u32 cb_qid[CB_SLOTS];
static int cb_n;
static int cb_alloc(u32 qid, u32 cb)
{
    int s;
    if (cb_n >= CB_SLOTS) return -1;
    s = cb_n++;
    cb_qid[s] = qid; orig_cb[s] = (cb4_fn)cb;
    return s;
}
/* stack_scan: resolve main-thread SP/PC from /proc stat and scan live stack */
static char mapsbuf[262144];
static char statbuf[1024];
struct maprng { u32 lo, hi; char *name; };
static struct maprng rngs[600];
static int nrng;

static int parse_hex(char **pp, u32 *out)
{
    char *p = *pp; u32 v = 0; int any = 0;
    while (*p) {
        char c = *p;
        if (c >= '0' && c <= '9') v = v*16 + (c-'0');
        else if (c >= 'a' && c <= 'f') v = v*16 + (c-'a'+10);
        else if (c >= 'A' && c <= 'F') v = v*16 + (c-'A'+10);
        else break;
        p++; any = 1;
    }
    *pp = p; *out = v; return any;
}

static int read_file(const char *path, char *buf, int cap)
{
    long fd, n, total = 0;
    fd = sc3(NR_OPEN, (long)path, 0, 0);
    if (fd < 0) return -1;
    while (total < cap - 1) {
        n = sc3(NR_READ, fd, (long)(buf + total), cap - 1 - total);
        if (n <= 0) break;
        total += n;
    }
    sc3(NR_CLOSE, fd, 0, 0);
    buf[total] = 0;
    return (int)total;
}

static int addr_readable(u32 a, u32 len)
{
    long total; char *p, *endp;
    total = read_file("/proc/self/maps", mapsbuf, sizeof(mapsbuf));
    if (total <= 0) return 0;
    p = mapsbuf; endp = mapsbuf + total;
    while (p < endp) {
        char *nl = p; while (nl < endp && *nl != '\n') nl++;
        {
            char *q = p; u32 lo = 0, hi = 0;
            if (parse_hex(&q, &lo) && *q == '-') {
                q++;
                if (parse_hex(&q, &hi) && *q == ' ' && q[1] == 'r') {
                    if (a >= lo && a < hi && hi - a >= len) return 1;
                }
            }
        }
        p = nl + 1;
    }
    return 0;
}

static void dump_msg_payload(u32 a, u32 len)
{
    const unsigned char *p;
    u32 i;
    if (!addr_readable(a, len)) { s(" |unmapped "); h(a); return; }
    p = (const unsigned char *)a;
    s(" |");
    for (i = 0; i < len && ln < 300; i++) {
        unsigned char c = p[i];
        lb[ln++] = (c >= 32 && c < 127) ? (char)c : '.';
    }
}

static u32 parse_dec_after(char *p, int idx)
{
    int i;
    for (i = 0; i < idx; i++) {
        while (*p && *p != ' ') p++;
        if (!*p) return 0;
        p++;
    }
    {
        u32 v = 0; int any = 0;
        while (*p >= '0' && *p <= '9') { v = v*10 + (*p - '0'); p++; any = 1; }
        return any ? v : 0;
    }
}

static void stack_scan(void)
{
    static int done;
    long total; char *p, *endp;
    u32 stk_lo = 0, stk_hi = 0, sp = 0, pc = 0;
    int hits = 0;
    if (done) return; done = 1;
    total = read_file("/proc/self/maps", mapsbuf, sizeof(mapsbuf));
    if (total <= 0) { beg("SCAN no-maps"); f(); return; }
    nrng = 0;
    p = mapsbuf; endp = mapsbuf + total;
    while (p < endp && nrng < 600) {
        char *nl = p; while (nl < endp && *nl != '\n') nl++;
        if (nl < endp) *nl = 0;
        {
            char *q = p; u32 lo = 0, hi = 0;
            if (parse_hex(&q, &lo) && *q == '-') {
                q++;
                if (parse_hex(&q, &hi)) {
                    int f;
                    for (f = 0; f < 5 && *q; f++) { while (*q && *q != ' ') q++; while (*q == ' ') q++; }
                    if (*q) {
                        rngs[nrng].lo = lo; rngs[nrng].hi = hi; rngs[nrng].name = q;
                        if (str_in(q, "[stack]")) { stk_lo = lo; stk_hi = hi; }
                        nrng++;
                    }
                }
            }
        }
        p = nl + 1;
    }
    if (!stk_lo || stk_hi <= stk_lo) { beg("SCAN no-stack"); f(); return; }
    /* own pid == main thread tid; read its saved user SP/PC */
    {
        char path[64]; int n = 0; long pid = sc3(20, 0, 0, 0); /* getpid */
        const char *a = "/proc/self/task/"; const char *b = "/stat";
        char num[12]; int k = 0;
        while (a[n]) { path[n] = a[n]; n++; }
        if (pid == 0) num[k++] = '0';
        while (pid > 0) { num[k++] = '0' + (pid % 10); pid /= 10; }
        while (k > 0) path[n++] = num[--k];
        k = 0; while (b[k]) { path[n++] = b[k]; k++; }
        path[n] = 0;
        if (read_file(path, statbuf, sizeof(statbuf)) > 0) {
            char *rp = statbuf;
            while (*rp && *rp != ')') rp++;
            if (*rp == ')') rp++;
            while (*rp == ' ') rp++;
            /* after ')': fields 3.. ; kstkesp = field 29, kstkeip = 30 */
            sp = parse_dec_after(rp, 26);
            pc = parse_dec_after(rp, 27);
        }
    }
    beg("SCAN stack "); h(stk_lo); s("-"); h(stk_hi); s(" sp="); h(sp); s(" pc="); h(pc); f();
    {
        u32 start = sp;
        u32 addr;
        if (start < stk_lo || start >= stk_hi) start = stk_hi - 0x800;
        for (addr = start & ~3u; addr < stk_hi; addr += 4) {
            u32 w = *(volatile u32 *)addr;
            int i;
            for (i = 0; i < nrng; i++) {
                if (w >= rngs[i].lo && w < rngs[i].hi) {
                    u32 off = w - rngs[i].lo;
                    if ((rngs[i].name[0] == '/' || rngs[i].name[0] == '[') && off < 0x2000000u) {
                        beg("STK "); h(addr); s(" -> "); s(rngs[i].name); s("+"); h(off); f();
                        hits++;
                    }
                    break;
                }
            }
            if (hits >= 120) break;
        }
    }
    beg("SCAN done hits="); d(hits); f();
}


static void im_dispatch_on(void)
{
    static int done;
    u32 msg[4];
    s32 (*fn)(u32);
    s32 r;
    u32 qid;
    if (done) return; done = 1;
    qid = *(volatile u32 *)(0x15438u + 0x2c);
    beg("IMD mainq="); h(qid); f();
    msg[0] = 0x0092015au; msg[1] = 0; msg[2] = 0; msg[3] = 0;
    fn = (s32 (*)(u32))(0x9c90u | 1u);
    beg("IMD call 92015a"); f();
    r = fn((u32)msg);
    beg("IMD ret="); d(r); f();
}

static void cb_log(u32 qid, u32 a, u32 b, u32 c, u32 d4, int slot)
{
    u32 cand[4]; u32 *m = 0; int i;
    cand[0]=a; cand[1]=b; cand[2]=c; cand[3]=d4;
    for (i=0;i<4;i++) {
        u32 v = cand[i];
        if (v > 0x1000u && v < 0x80000000u) {
            u32 w = *(volatile u32 *)v;
            if ((w >> 16) == 0x0092u || (w >> 16) == 0x0094u || (w >> 16) == 0x008fu) { m = (u32 *)v; break; }
        }
    }
    beg("CB q="); h(qid); s(" a="); h(a); s(" b="); h(b); s(" c="); h(c); s(" d="); h(d4); s(" cb="); h((u32)orig_cb[slot]);
    { u32 lb = 0; const char *ln = addr_lib2((u32)orig_cb[slot], &lb); if (ln) { s(" lib="); s(ln); s(" lbase="); h(lb); } }
    if (m) { s(" ty="); h(m[0]); s(" src="); h(m[-1]); s(" w="); log_words(m, 8); }
    f();
    if (m && m[0] == 0x00920131u && m[1] == 0x5343414eu) { stack_scan(); im_dispatch_on(); }
}
static s32 cb_w0(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[0],a,b,c,d4,0); return orig_cb[0]?orig_cb[0](a,b,c,d4):0; }
static s32 cb_w1(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[1],a,b,c,d4,1); return orig_cb[1]?orig_cb[1](a,b,c,d4):0; }
static s32 cb_w2(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[2],a,b,c,d4,2); return orig_cb[2]?orig_cb[2](a,b,c,d4):0; }
static s32 cb_w3(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[3],a,b,c,d4,3); return orig_cb[3]?orig_cb[3](a,b,c,d4):0; }
static s32 cb_w4(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[4],a,b,c,d4,4); return orig_cb[4]?orig_cb[4](a,b,c,d4):0; }
static s32 cb_w5(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[5],a,b,c,d4,5); return orig_cb[5]?orig_cb[5](a,b,c,d4):0; }
static s32 cb_w6(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[6],a,b,c,d4,6); return orig_cb[6]?orig_cb[6](a,b,c,d4):0; }
static s32 cb_w7(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[7],a,b,c,d4,7); return orig_cb[7]?orig_cb[7](a,b,c,d4):0; }
static s32 cb_w8(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[8],a,b,c,d4,8); return orig_cb[8]?orig_cb[8](a,b,c,d4):0; }
static s32 cb_w9(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[9],a,b,c,d4,9); return orig_cb[9]?orig_cb[9](a,b,c,d4):0; }
static s32 cb_w10(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[10],a,b,c,d4,10); return orig_cb[10]?orig_cb[10](a,b,c,d4):0; }
static s32 cb_w11(u32 a,u32 b,u32 c,u32 d4){ cb_log(cb_qid[11],a,b,c,d4,11); return orig_cb[11]?orig_cb[11](a,b,c,d4):0; }
static cb4_fn cb_wrappers[CB_SLOTS] = { cb_w0, cb_w1, cb_w2, cb_w3, cb_w4, cb_w5, cb_w6, cb_w7, cb_w8, cb_w9, cb_w10, cb_w11 };
s32 osal_reg_msg_queue_cb(u32 a, u32 b, u32 c, u32 d4)
{
    int slot;
    if (!r_reg) { u32 bb = osal_base(); if (bb) r_reg = (reg_fn)(bb + OFF_REG); }
    if (!r_reg) { nodl("reg", a); return -1; }
    if (interesting(a) || interesting(c)) {
        beg("REG a="); h(a); s(" b="); h(b); s(" c="); h(c); s(" d="); h(d4);
        { u32 lb = 0; const char *ln = addr_lib2(c, &lb); if (ln) { s(" lib="); s(ln); s(" lbase="); h(lb); } }
        f();
        if (0 && (a == 0x00924126u || a == 0x00924127u || a == 0x0094016au) && c) {
            slot = cb_alloc(a, c);
            if (slot >= 0) c = (u32)cb_wrappers[slot];
        }
    }
    return r_reg(a,b,c,d4);
}
s32 osal_reg_sync_msg_queue_cb(u32 a, u32 b, u32 c, u32 d4)
{
    if (!r_regs) { u32 bb = osal_base(); if (bb) r_regs = (reg_fn)(bb + OFF_REGS); }
    if (!r_regs) { nodl("regs", a); return -1; }
    if (interesting(a) || interesting(c)) { beg("REGS a="); h(a); s(" b="); h(b); s(" c="); h(c); f(); }
    return r_regs(a,b,c,d4);
}
s32 osal_dynamic_id(u32 a, u32 b, u32 c, u32 d4)
{
    s32 ret;
    if (!r_dyn) { u32 bb = osal_base(); if (bb) r_dyn = (reg_fn)(bb + OFF_DYN); }
    if (!r_dyn) { nodl("dyn", a); return -1; }
    ret = r_dyn(a,b,c,d4);
    { u32 ra = (u32)__builtin_return_address(0); beg("DYN a="); h(a); s(" b="); h(b); s(" c="); h(c); s(" d="); h(d4); s(" ret="); d(ret); { const char *ln = addr_lib2(ra, 0); if (ln) { s(" lib="); s(ln); } } f(); }
    if (ret == 0 && a == 0x2au && b == 0x94u && c) {
        gui_qid = *(u32 *)c;
        gui_stage = 0;
        beg("GUIQ "); h(gui_qid); f();
    }
    return ret;
}

/* Inline hook for DefRootView::ActionEventMapBase::addAction (internal calls). */
#define NR_MPROTECT 125
static u32 ah_addr, ah_orig[2];
static int ah_installed, ah_count;
static void ah_install(void);
static s32 ah_hook(u32 th, s32 id, u32 act)
{
    int n;
    // log id + action object pointer + vtable first entries
    beg("ACT add id="); h((u32)id); s(" act="); h(act);
    if (act >= 0x1000 && act < 0x80000000u) {
        u32 *a = (u32 *)act;
        u32 vt = a[0];
        s(" vt="); h(vt);
        if (vt >= 0x04000000u && vt < 0x0c000000u) {
            u32 *v = (u32 *)vt;
            for (n = 0; n < 3; n++) { s(" v"); d(n); s("="); h(v[n]); }
        }
        s(" w4="); h(a[4]);
    }
    f();
    ah_count++;
    /* restore, call original, reinstall */
    *(volatile u32 *)ah_addr = ah_orig[0];
    *(volatile u32 *)(ah_addr + 4) = ah_orig[1];
    __builtin___clear_cache((char *)ah_addr, (char *)ah_addr + 8);
    {
        s32 (*fn)(u32,s32,u32) = (void *)(ah_addr | 1u);
        s32 r = fn(th, id, act);
        /* reinstall jump */
        *(volatile u32 *)ah_addr = 0xF000F8DFu;
        *(volatile u32 *)(ah_addr + 4) = (u32)&ah_hook | 1u;
        __builtin___clear_cache((char *)ah_addr, (char *)ah_addr + 8);
        return r;
    }
}
static void ah_install(void)
{
    u32 base;
    return; /* inline hook disabled: it runs off into the literal pool on call */
    if (ah_installed) return;
    base = find_lib("libObj.so");
    if (!base) return;
    ah_addr = base + 0x12a474u;
    sc3(NR_MPROTECT, ah_addr & ~0xfffu, 0x2000, 7);
    ah_orig[0] = *(volatile u32 *)ah_addr;
    ah_orig[1] = *(volatile u32 *)(ah_addr + 4);
    *(volatile u32 *)ah_addr = 0xF000F8DFu;
    *(volatile u32 *)(ah_addr + 4) = (u32)&ah_hook | 1u;
    __builtin___clear_cache((char *)ah_addr, (char *)ah_addr + 8);
    ah_installed = 1;
    beg("ACT hooked base="); h(base); f();
}

/* DefRootView action map: watch action IDs registered/executed. */
static void kill_mod(const char *n) { sc3(129, (long)n, 0, 0); }
/* Stop the kernel log writer and key scanner as early as possible: their
 * synchronous vfat writes and phantom key stream stall the app in QEMU. */
void __attribute__((constructor)) shim_ctor(void)
{
    kill_mod("kikilog");
}
static void kill_modules_once(void)
{
    static int done;
    if (done) return;
    done = 1;
    kill_mod("kikilog");
    beg("MODKILL done"); f();
}
s32 _ZN11DefRootView18ActionEventMapBase9addActionEiPNS_10ActionBaseE(u32 th, s32 id, u32 act)
{
    u32 bb = find_lib("libObj.so"); s32 (*fn)(u32,s32,u32) = (void *)(bb + 0x12a475u); s32 r;
    beg("ACT add th="); h(th); s(" id="); h((u32)id); s(" act="); h(act); f();
    kill_modules_once();
    r = bb ? fn(th,id,act) : -1;
    return r;
}
s32 _ZN11DefRootView18ActionEventMapBase10execActionEi(u32 th, s32 id)
{
    u32 bb = find_lib("libObj.so"); s32 (*fn)(u32,s32) = (void *)(bb + 0x12a415u); s32 r;
    beg("ACT exec th="); h(th); s(" id="); h((u32)id); f();
    r = bb ? fn(th,id) : -1;
    beg("ACT exec<- "); d(r); f();
    return r;
}

/* View manager observability: log view opens and UI event pushes. */
static void log_cstr(u32 a, int max)
{
    const char *p; int i;
    if (!a || !addr_readable(a, 1)) { s(" <bad>"); return; }
    p = (const char *)a;
    for (i = 0; i < max && ln < 300; i++) {
        char c = p[i];
        if (!c) break;
        lb[ln++] = (c >= 32 && c < 127) ? c : '.';
    }
}
s32 _ZN13viewManagerIf8openViewEPKci(u32 mgr, u32 name, s32 mode)
{
    u32 bb = find_lib("libObj.so"); s32 (*fn)(u32,u32,s32) = (void *)(bb + 0x1349ddu); s32 r;
    beg("OVIEW mgr="); h(mgr); s(" mode="); d(mode); s(" name="); log_cstr(name, 64); f();
    r = bb ? fn(mgr, name, mode) : -1;
    beg("OVIEW< r="); d(r); f();
    return r;
}
s32 _ZN4View9pushEventEP5Event(u32 th, u32 ev)
{
    u32 bb = find_lib("libObj.so"); void (*fn)(u32,u32) = (void *)(bb + 0x862835u);
    kill_modules_once();
    ui_tid = tid(); ui_tid_seen = 1;
    beg("VEV th="); h(th); s(" ev="); h(ev);
    if (ev && addr_readable(ev, 16)) { u32 *m = (u32 *)ev; s(" w="); log_words(m, 4); }
    f();
    if (bb) fn(th, ev);
    if (bb) {
        u32 pvm = *(volatile u32 *)(bb + 0x13289f4u);
        if (pvm) {
            static int opened;
            if (!opened) {
                s32 (*ov)(u32,u32,s32) = (void *)(bb + 0x1349ddu);
                opened = 1;
                beg("OVCALL pvm="); h(pvm); f();
                { s32 r = ov(pvm, (u32)"view/BASE_MENU", 0); beg("OVRET r="); d(r); f(); }
            }
        }
    }
    return 0;
}

/* UI subsystem entry points (libObj). */
s32 ObjDisplay_Create(u32 a, u32 b)
{
    u32 bb = find_lib("libObj.so"); s32 (*fn)(u32,u32) = (void *)(bb + 0xcb2d9du); s32 r;
    beg("UI ObjDisplay_Create a="); h(a); s(" b="); h(b); f();
    r = bb ? fn(a,b) : -1;
    beg("UI ObjDisplay_Create<- "); d(r); f();
    return r;
}
s32 ObjDisplay_Destroy(u32 a)
{
    u32 bb = find_lib("libObj.so"); s32 (*fn)(u32) = (void *)(bb + 0xcb2dc9u); s32 r;
    beg("UI ObjDisplay_Destroy a="); h(a); f();
    r = bb ? fn(a) : -1;
    return r;
}
s32 ObjDisplay_RegisterCommand(u32 a, u32 b, u32 c, u32 d4)
{
    u32 bb = find_lib("libObj.so"); s32 (*fn)(u32,u32,u32,u32) = (void *)(bb + 0xcb2e89u); s32 r;
    beg("UI ObjDisplay_RegisterCommand a="); h(a); s(" b="); h(b); s(" c="); h(c); f();
    r = bb ? fn(a,b,c,d4) : -1;
    return r;
}
s32 ModelRootToInstance(u32 a)
{
    u32 bb = find_lib("libObj.so"); s32 (*fn)(u32) = (void *)(bb + 0x6b6535u); s32 r;
    beg("UI ModelRoot a="); h(a); f();
    r = bb ? fn(a) : -1;
    beg("UI ModelRoot<- "); h((u32)r); f();
    return r;
}

/* libObj input entry points: learn the InfraInput queue id and callback. */
s32 _ZN10InfraInput9SetOsalIdEPii(u32 th, u32 p, s32 flag)
{
    u32 b = find_lib("libObj.so"); s32 (*fn)(u32,u32,s32) = (void *)(b + 0x884065u); s32 r;
    beg("INPUT SetOsalId th="); h(th); s(" p="); h(p); s(" v="); h(p ? *(u32 *)p : 0); s(" f="); d(flag); f();
    r = b ? fn(th,p,flag) : -1;
    beg("INPUT SetOsalId<- "); d(r); s(" v="); h(p ? *(u32 *)p : 0); f();
    return r;
}
s32 _ZN10InfraInput11SetCallbackEPiPFviP16_InfraInputEventE(u32 th, u32 p, u32 cb)
{
    u32 b = find_lib("libObj.so"); s32 (*fn)(u32,u32,u32) = (void *)(b + 0x8840d1u); s32 r;
    beg("INPUT SetCallback th="); h(th); s(" p="); h(p); s(" cb="); h(cb); f();
    r = b ? fn(th,p,cb) : -1;
    beg("INPUT SetCallback<- "); d(r); s(" v="); h(p ? *(u32 *)p : 0); f();
    return r;
}
s32 _ZN12InputService11callback_tpEiP16_InfraInputEvent(u32 th, u32 a, u32 ev)
{
    u32 b = find_lib("libObj.so"); s32 (*fn)(u32,u32,u32) = (void *)(b + 0x116d29u); s32 r;
    if (ev) { beg("INPUT cb_tp th="); h(th); s(" a="); d((s32)a); s(" code="); h(((u32 *)ev)[0]); s(" w="); log_words((u32 *)ev, 8); f(); }
    r = b ? fn(th,a,ev) : -1;
    return r;
}

/* libObj GRM (Sugilite userspace) entry points, to learn bitmap addresses. */
static u32 grm_base(void) { return find_lib("libObj.so"); }
#define GRM_CREATE  0x64f765u
#define GRM_PHYS    0x64f9b9u
#define GRM_ONBMP   0x65056du
#define GRM_INIT    0x6509a5u
s32 GRM_bitmapCreate(u32 bw, u32 bh, u32 fmt, u32 d4)
{
    u32 b = grm_base(); s32 (*fn)(u32,u32,u32,u32) = (void *)(b + GRM_CREATE); s32 r;
    beg("GRM create w="); h(bw); s(" h="); h(bh); s(" f="); h(fmt); s(" d="); h(d4); f();
    r = b ? fn(bw,bh,fmt,d4) : -1;
    beg("GRM create<- "); h((u32)r); f();
    return r;
}
u32 GRM_bitmapGetPhysicalAddress(u32 bm)
{
    u32 b = grm_base(); u32 (*fn)(u32) = (void *)(b + GRM_PHYS); u32 r;
    r = b ? fn(bm) : 0;
    beg("GRM phys bm="); h(bm); s(" -> "); h(r); f();
    return r;
}
u32 GRM_screenGetOnBitmap(void)
{
    u32 b = grm_base(); u32 (*fn)(void) = (void *)(b + GRM_ONBMP); u32 r;
    r = b ? fn() : 0;
    beg("GRM onbmp -> "); h(r); f();
    return r;
}
s32 grm_init(u32 a, u32 b2, u32 c, u32 d4)
{
    u32 b = grm_base(); s32 (*fn)(u32,u32,u32,u32) = (void *)(b + GRM_INIT); s32 r;
    beg("GRM init a="); h(a); s(" b="); h(b2); s(" c="); h(c); s(" d="); h(d4); f();
    r = b ? fn(a,b2,c,d4) : -1;
    beg("GRM init<- "); h((u32)r); f();
    return r;
}

/* MprIf power/display entry points (libmpr.so symbol offsets). */
typedef s32 (*mpr1_fn)(u32);
typedef s32 (*mpr2_fn)(u32, u32);
typedef s32 (*mpr3_fn)(u32, u32, u32);
static mpr1_fn m_poweron, m_poweroff, m_snap;
static mpr2_fn m_resume;
static mpr3_fn m_dp1, m_dp2;
static int mpr_dp_log;
#define MPR_POWERON  0x864729u
#define MPR_POWEROFF 0x864751u
#define MPR_RESUME   0x8654a1u
#define MPR_SNAP     0x85eac9u
#define MPR_DP1      0x866e39u
#define MPR_DP2      0x866e69u

s32 _ZN5MprIf7PowerOnEm(u32 a)
{
    s32 ret; u32 b;
    if (!m_poweron) { b = find_lib("libmpr.so"); if (b) m_poweron = (mpr1_fn)(b + MPR_POWERON); }
    beg("MPR PowerOn a="); h(a); f();
    ret = m_poweron ? m_poweron(a) : -1;
    beg("MPR PowerOn<- "); d(ret); f();
    return ret;
}
s32 _ZN5MprIf8PowerOffEm(u32 a)
{
    s32 ret; u32 b;
    if (!m_poweroff) { b = find_lib("libmpr.so"); if (b) m_poweroff = (mpr1_fn)(b + MPR_POWEROFF); }
    beg("MPR PowerOff a="); h(a); f();
    ret = m_poweroff ? m_poweroff(a) : -1;
    return ret;
}
s32 _ZN5MprIf6ResumeEbN3MPR8BootTypeE(u32 a, u32 c)
{
    s32 ret; u32 b;
    if (!m_resume) { b = find_lib("libmpr.so"); if (b) m_resume = (mpr2_fn)(b + MPR_RESUME); }
    beg("MPR Resume a="); h(a); s(" t="); h(c); f();
    ret = m_resume ? m_resume(a, c) : -1;
    beg("MPR Resume<- "); d(ret); f();
    return ret;
}
s32 _ZN5MprIf16SnapShotBootInitEPNS_21SnapShotBootInitParamE(u32 p)
{
    s32 ret; u32 b;
    if (!m_snap) { b = find_lib("libmpr.so"); if (b) m_snap = (mpr1_fn)(b + MPR_SNAP); }
    beg("MPR SnapBootInit p="); h(p); f();
    ret = m_snap ? m_snap(p) : -1;
    return ret;
}
s32 _ZN5MprIf10DrawPixelsEmN3MPR9ColorModeEm(u32 a, u32 cm, u32 c)
{
    s32 ret; u32 b;
    if (!m_dp1) { b = find_lib("libmpr.so"); if (b) m_dp1 = (mpr3_fn)(b + MPR_DP1); }
    if (mpr_dp_log < 12) { beg("MPR DrawPix a="); h(a); s(" cm="); h(cm); s(" n="); h(c); f(); mpr_dp_log++; }
    ret = m_dp1 ? m_dp1(a, cm, c) : -1;
    return ret;
}
s32 _ZN5MprIf10DrawPixelsEmmN3MPR9ColorModeE(u32 a, u32 b2, u32 cm)
{
    s32 ret; u32 b;
    if (!m_dp2) { b = find_lib("libmpr.so"); if (b) m_dp2 = (mpr3_fn)(b + MPR_DP2); }
    if (mpr_dp_log < 12) { beg("MPR DrawPix2 a="); h(a); s(" b="); h(b2); s(" cm="); h(cm); f(); mpr_dp_log++; }
    ret = m_dp2 ? m_dp2(a, b2, cm) : -1;
    return ret;
}
