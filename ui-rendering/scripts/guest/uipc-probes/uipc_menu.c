/* Inject MENU key press/release into the InfraInput UIPC queue. */
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

static void send_key(u32 qid, u32 code, u32 status, u32 prev)
{
    void *m = 0;
    if (osal_valloc_msg_wait(qid, &m, 0x20, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = qid;
        p[0] = code;      /* 0x00 event_code */
        p[1] = 0;         /* 0x04 tp_y | tp_x<<16 */
        p[2] = status;    /* 0x08 status */
        p[3] = prev;      /* 0x0c prev_status */
        p[4] = 0;         /* 0x10 ex_info */
        p[5] = 0;         /* 0x14 repeat */
        p[6] = 1;         /* 0x18 from_type */
        p[7] = 0;         /* 0x1c inh + padding */
        osal_snd_msg(qid, m);
    }
}

void __attribute__((constructor)) inject_menu(void)
{
    struct timespec ts;
    send_key(0x005503e0u, 0x1abu, 1, 0);
    ts.tv_sec = 0; ts.tv_nsec = 150000000L;
    sc_nanosleep(&ts);
    send_key(0x005503e0u, 0x1abu, 0, 1);
}
