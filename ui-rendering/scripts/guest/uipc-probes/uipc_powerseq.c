/* Drive the AP C3P power sequence: ON, then 0x920143 power-state events. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_msg(u32 qid, void *msg);

struct timespec { long tv_sec; long tv_nsec; };
static long sc_nanosleep(struct timespec *ts)
{
    register long r0 __asm__("r0") = 0;
    register long r1 __asm__("r1") = (long)ts;
    register long r7 __asm__("r7") = 162;
    __asm__ volatile("svc #0" : "+r"(r0) : "r"(r1), "r"(r7) : "memory");
    return r0;
}
static void send_msg(u32 type, u32 p1, u32 p2, u32 p3)
{
    void *m = 0;
    if (osal_valloc_msg_wait(0x0094016au, &m, 0x10, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = 0x0094016au;
        p[0] = type; p[1] = p1; p[2] = p2; p[3] = p3;
        osal_snd_msg(0x0094016au, m);
    }
}
static void nap(long ms)
{
    struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (ms % 1000) * 1000000L;
    sc_nanosleep(&ts);
}

void __attribute__((constructor)) power_seq(void)
{
    send_msg(0x0092015au, 0, 0, 0);          /* UPM_CMD_ON */
    nap(300);
    send_msg(0x00920143u, 0x27, 0, 0);       /* power-state event, type 0x27 */
    nap(300);
    send_msg(0x00920143u, 0x1b, 0, 0);       /* type 0x1b */
    nap(300);
    send_msg(0x00920143u, 0x01, 0, 0);       /* type 1 */
}
