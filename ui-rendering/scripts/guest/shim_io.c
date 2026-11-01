/* Minimal ioctl/open tracer for /dev/dmpgles2, loaded via /etc/ld.so.preload.
 * Uses raw ARM EABI syscalls so it needs no libc/libdl. */
typedef unsigned int u32;
typedef int s32;

#define NR_WRITE 4
#define NR_OPEN  5
#define NR_CLOSE 6
#define NR_IOCTL 54

static long sc3(long n, long a, long b, long c)
{
    register long r0 __asm__("r0") = a;
    register long r1 __asm__("r1") = b;
    register long r2 __asm__("r2") = c;
    register long r7 __asm__("r7") = n;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7)
                     : "memory");
    return r0;
}

static char lb[96]; static int ln;
static void s(const char *p){ while(*p && ln<95) lb[ln++]=*p++; }
static void h(u32 v){ static const char d[]="0123456789abcdef"; int i; for(i=28;i>=0;i-=4) if(ln<95) lb[ln++]=d[(v>>i)&15]; }
static void d(s32 v){ char t[12]; int n=0; if(v<0){ if(ln<95) lb[ln++]='-'; v=-v;} if(!v){ if(ln<95) lb[ln++]='0'; return;} while(v&&n<12){ t[n++]='0'+v%10; v/=10;} while(n) if(ln<95) lb[ln++]=t[--n]; }
static void f(void){ if(ln>0){ sc3(NR_WRITE,2,(long)lb,ln); ln=0; } }
static void beg(void){ ln=0; s("[IO] "); }

static int dmpfd = -1;

static void io_ctor(void) __attribute__((constructor));
static void io_ctor(void)
{
    beg(); s("LOADED\n"); f();
}

int open(const char *path, int flags, ...)
{
    int fd;
    __builtin_va_list ap;
    int mode = 0;
    if (flags & 0x40) {
        __builtin_va_start(ap, flags);
        mode = __builtin_va_arg(ap, int);
        __builtin_va_end(ap);
    }
    fd = (int)sc3(NR_OPEN, (long)path, flags, mode);
    if (path && path[0]=='/' && path[1]=='d' && path[2]=='e' && path[3]=='v' &&
        path[4]=='/' && path[5]=='d' && path[6]=='m' && path[7]=='p') {
        const char *q = "dmpgles2"; int i = 0;
        while (q[i] && path[5+i] == q[i]) i++;
        if (!q[i]) {
            dmpfd = fd;
            beg(); s("OPEN dmp fd="); d(fd); s(" flags="); h((u32)flags); f();
        }
    }
    return fd;
}

int open64(const char *path, int flags, ...) { return open(path, flags); }

int ioctl(int fd, unsigned long req, void *arg)
{
    int ret;
    if (fd >= 0 && fd == dmpfd) {
        beg(); s("IOCTL> req="); h((u32)req);
        s(" arg="); h((u32)arg);
        if ((u32)req == 0xc0048203u && (u32)arg >= 0x1000u &&
            (u32)arg < 0x80000000u) {
            s(" val="); h(*(volatile u32 *)arg);
        }
        f();
    }
    ret = (int)sc3(NR_IOCTL, fd, (long)req, (long)arg);
    if (fd >= 0 && fd == dmpfd) {
        beg(); s("IOCTL< req="); h((u32)req);
        s(" ret="); d(ret); f();
    }
    return ret;
}

int close(int fd)
{
    int ret = (int)sc3(NR_CLOSE, fd, 0, 0);
    if (fd == dmpfd) dmpfd = -1;
    return ret;
}
