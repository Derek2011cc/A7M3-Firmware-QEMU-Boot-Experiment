/* UIPC + device observability shim for the A7M3 QEMU experiment.
 * Logs queue ids/message types and open/mmap/ioctl on graphics devices to
 * stderr (the guest console), then calls the real functions via dlsym. */
typedef unsigned int u32;
typedef int s32;

extern void *dlsym(void *handle, const char *symbol);
extern long write(int fd, const void *buf, unsigned long count);

#define RTLD_NEXT ((void *)-1L)

static char logbuf[200];
static int logn;

static void log_reset(void) { logn = 0; }
static void log_ch(char c) { if (logn < (int)sizeof(logbuf) - 1) logbuf[logn++] = c; }
static void log_str(const char *s) { while (*s) log_ch(*s++); }
static void log_hex(u32 v)
{
    static const char h[] = "0123456789abcdef";
    int i;
    for (i = 28; i >= 0; i -= 4) log_ch(h[(v >> i) & 0xf]);
}
static void log_dec(s32 v)
{
    char tmp[12];
    int n = 0;
    if (v < 0) { log_ch('-'); v = -v; }
    if (v == 0) { log_ch('0'); return; }
    while (v > 0 && n < (int)sizeof(tmp)) { tmp[n++] = '0' + (v % 10); v /= 10; }
    while (n > 0) log_ch(tmp[--n]);
}
static void log_flush(void)
{
    if (logn > 0) {
        write(2, logbuf, (unsigned long)logn);
        logn = 0;
    }
}
static void log_more_str(const char *s) { log_str(s); }
static void log_words(const u32 *p, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        log_str(" "); log_hex(p[i]);
    }
}

static void *resolve(const char *name, void **slot)
{
    if (*slot == 0) *slot = dlsym(RTLD_NEXT, name);
    return *slot;
}

typedef long (*syscall_fn)(long, ...);
static syscall_fn real_syscall;
static int my_tid(void)
{
    resolve("syscall", (void **)&real_syscall);
    if (!real_syscall) return 0;
    return (int)real_syscall(224L); /* gettid on ARM EABI */
}
static void log_begin(const char *tag)
{
    log_reset();
    log_str("[SHIM t"); log_dec(my_tid()); log_str("] "); log_str(tag);
}

/* ---- UIPC ---- */
typedef s32 (*rcv_fn)(u32 qid, void **msg, s32 timeout);
typedef s32 (*snd_fn)(u32 qid, void *msg);
typedef s32 (*valloc_fn)(u32 qid, void **msg, u32 size, u32 flags);
typedef s32 (*reg_fn)(u32 a, u32 b, u32 c, u32 d);

static rcv_fn real_rcv;
static snd_fn real_snd;
static snd_fn real_snd_sync;
static valloc_fn real_valloc;
static reg_fn real_reg;
static reg_fn real_reg_sync;
static reg_fn real_dyn;

s32 osal_rcv_msg_tmo(u32 qid, void **msg, s32 timeout)
{
    s32 ret;
    u32 ra = (u32)__builtin_return_address(0);
    resolve("osal_rcv_msg_tmo", (void **)&real_rcv);
    log_begin("RCV qid="); log_hex(qid);
    log_str(" to="); log_dec(timeout);
    log_str(" ra="); log_hex(ra); log_flush();
    ret = real_rcv ? real_rcv(qid, msg, timeout) : -1;
    log_begin("RCVRET "); log_dec(ret);
    log_str(" qid="); log_hex(qid); log_flush();
    if (ret == 0 && msg && *msg) {
        u32 *m = (u32 *)*msg;
        log_begin("RCVOK type="); log_hex(m[0]);
        log_str(" src="); log_hex(m[-1]);
        log_str(" body="); log_words(m, 6); log_flush();
    }
    return ret;
}

s32 osal_snd_msg(u32 qid, void *msg)
{
    s32 ret;
    u32 ra = (u32)__builtin_return_address(0);
    resolve("osal_snd_msg", (void **)&real_snd);
    if (msg) {
        u32 *m = (u32 *)msg;
        log_begin("SND qid="); log_hex(qid);
        log_str(" type="); log_hex(m[0]);
        log_str(" src="); log_hex(m[-1]);
        log_str(" ra="); log_hex(ra);
        log_str(" body="); log_words(m, 6); log_flush();
    }
    ret = real_snd ? real_snd(qid, msg) : -1;
    log_begin("SNDRET "); log_dec(ret);
    log_str(" qid="); log_hex(qid); log_flush();
    return ret;
}

s32 osal_snd_sync_msg(u32 qid, void *msg)
{
    s32 ret;
    u32 ra = (u32)__builtin_return_address(0);
    resolve("osal_snd_sync_msg", (void **)&real_snd_sync);
    if (msg) {
        u32 *m = (u32 *)msg;
        log_begin("SSND qid="); log_hex(qid);
        log_str(" type="); log_hex(m[0]);
        log_str(" src="); log_hex(m[-1]);
        log_str(" ra="); log_hex(ra);
        log_str(" body="); log_words(m, 6); log_flush();
    }
    ret = real_snd_sync ? real_snd_sync(qid, msg) : -1;
    if (msg) {
        u32 *m = (u32 *)msg;
        log_begin("SSNDRET "); log_dec(ret);
        log_str(" type="); log_hex(m[0]);
        log_str(" src="); log_hex(m[-1]);
        log_str(" body="); log_words(m, 6); log_flush();
    }
    return ret;
}

s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags)
{
    s32 ret;
    u32 ra = (u32)__builtin_return_address(0);
    resolve("osal_valloc_msg_wait", (void **)&real_valloc);
    log_begin("VALLOC qid="); log_hex(qid);
    log_str(" size="); log_dec((s32)size);
    log_str(" flags="); log_dec((s32)flags);
    log_str(" ra="); log_hex(ra); log_flush();
    ret = real_valloc ? real_valloc(qid, msg, size, flags) : -1;
    return ret;
}

s32 osal_reg_msg_queue_cb(u32 a, u32 b, u32 c, u32 d)
{
    u32 ra = (u32)__builtin_return_address(0);
    resolve("osal_reg_msg_queue_cb", (void **)&real_reg);
    log_begin("REG a="); log_hex(a); log_str(" b="); log_hex(b);
    log_str(" c="); log_hex(c); log_str(" d="); log_hex(d);
    log_str(" ra="); log_hex(ra); log_flush();
    return real_reg ? real_reg(a, b, c, d) : -1;
}

s32 osal_reg_sync_msg_queue_cb(u32 a, u32 b, u32 c, u32 d)
{
    u32 ra = (u32)__builtin_return_address(0);
    resolve("osal_reg_sync_msg_queue_cb", (void **)&real_reg_sync);
    log_begin("REGS a="); log_hex(a); log_str(" b="); log_hex(b);
    log_str(" c="); log_hex(c); log_str(" d="); log_hex(d);
    log_str(" ra="); log_hex(ra); log_flush();
    return real_reg_sync ? real_reg_sync(a, b, c, d) : -1;
}

s32 osal_dynamic_id(u32 a, u32 b, u32 c, u32 d)
{
    s32 ret;
    u32 ra = (u32)__builtin_return_address(0);
    resolve("osal_dynamic_id", (void **)&real_dyn);
    log_begin("DYN a="); log_hex(a); log_str(" b="); log_hex(b);
    log_str(" c="); log_hex(c); log_str(" d="); log_hex(d);
    log_str(" ra="); log_hex(ra); log_flush();
    ret = real_dyn ? real_dyn(a, b, c, d) : -1;
    log_begin("DYN-> "); log_hex((u32)ret);
    if (c >= 0x1000u && c < 0x80000000u) {
        u32 *p = (u32 *)c;
        log_str(" out="); log_hex(p[0]);
    }
    log_flush();
    return ret;
}

/* ---- device I/O ---- */
typedef int (*open_fn)(const char *, int, ...);
typedef void *(*mmap_fn)(void *, unsigned long, int, int, int, long);
typedef void *(*mmap64_fn)(void *, unsigned long, int, int, int, long long);
typedef int (*ioctl_fn)(int, unsigned long, void *);
typedef int (*close_fn)(int);

static open_fn real_open;
static mmap_fn real_mmap;
static mmap64_fn real_mmap64;
static ioctl_fn real_ioctl;
static close_fn real_close;

static int dmp_fd = -1;
static int fb_fd = -1;
static int mem_fds[24];
static int mem_n;

static int is_mem(int fd)
{
    int i;
    for (i = 0; i < mem_n; i++) if (mem_fds[i] == fd) return 1;
    return 0;
}
static int contains(const char *s, const char *sub)
{
    while (*s) {
        const char *a = s, *b = sub;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b) return 1;
        s++;
    }
    return 0;
}

int open(const char *path, int flags, ...)
{
    int fd, mode = 0;
    int tracked = 0;
    __builtin_va_list ap;
    resolve("open", (void **)&real_open);
    if (flags & 0x40) { /* O_CREAT */
        __builtin_va_start(ap, flags);
        mode = __builtin_va_arg(ap, int);
        __builtin_va_end(ap);
    }
    fd = real_open ? real_open(path, flags, mode) : -1;
    if (path && (contains(path, "dmpgles") || contains(path, "/dev/fb") ||
                 contains(path, "/dev/mem") || contains(path, "/dev/stream"))) {
        tracked = 1;
        if (contains(path, "dmpgles")) dmp_fd = fd;
        else if (contains(path, "/dev/fb")) fb_fd = fd;
        else if (contains(path, "/dev/mem") && fd >= 0 && mem_n < 24) mem_fds[mem_n++] = fd;
    }
    if (tracked) {
        log_begin("OPEN fd="); log_dec(fd);
        log_str(" flags="); log_hex((u32)flags);
        log_str(" "); log_str(path); log_flush();
    }
    return fd;
}

int open64(const char *path, int flags, ...)
{
    return open(path, flags);
}

void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, long off)
{
    void *ret;
    resolve("mmap", (void **)&real_mmap);
    ret = real_mmap ? real_mmap(addr, len, prot, flags, fd, off) : (void *)-1;
    if (fd == dmp_fd || fd == fb_fd || is_mem(fd)) {
        log_begin("MMAP fd="); log_dec(fd);
        log_str(" len="); log_hex((u32)len);
        log_str(" prot="); log_hex((u32)prot);
        log_str(" flags="); log_hex((u32)flags);
        log_str(" off="); log_hex((u32)off);
        log_str(" ret="); log_hex((u32)ret); log_flush();
    }
    return ret;
}

void *mmap64(void *addr, unsigned long len, int prot, int flags, int fd, long long off)
{
    void *ret;
    resolve("mmap64", (void **)&real_mmap64);
    ret = real_mmap64 ? real_mmap64(addr, len, prot, flags, fd, off) : (void *)-1;
    if (fd == dmp_fd || fd == fb_fd || is_mem(fd)) {
        log_begin("MMAP64 fd="); log_dec(fd);
        log_str(" len="); log_hex((u32)len);
        log_str(" prot="); log_hex((u32)prot);
        log_str(" flags="); log_hex((u32)flags);
        log_str(" off="); log_hex((u32)off);
        log_str(" ret="); log_hex((u32)ret); log_flush();
    }
    return ret;
}

int ioctl(int fd, unsigned long req, void *arg)
{
    int ret;
    resolve("ioctl", (void **)&real_ioctl);
    ret = real_ioctl ? real_ioctl(fd, req, arg) : -1;
    if (fd == dmp_fd || fd == fb_fd) {
        log_begin("IOCTL fd="); log_dec(fd);
        log_str(" req="); log_hex((u32)req);
        log_str(" arg="); log_hex((u32)arg);
        log_str(" ret="); log_dec(ret); log_flush();
    }
    return ret;
}

int close(int fd)
{
    int ret;
    resolve("close", (void **)&real_close);
    ret = real_close ? real_close(fd) : -1;
    if (fd == dmp_fd) dmp_fd = -1;
    if (fd == fb_fd) fb_fd = -1;
    return ret;
}
