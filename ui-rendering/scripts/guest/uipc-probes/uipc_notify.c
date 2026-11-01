/* Deliver synthetic UPM notifications to the app's UPM queue 0x00924126. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_msg(u32 qid, void *msg);

static void send_msg(u32 type, u32 w1, u32 w2, u32 w3, u32 w4, u32 w5, u32 w6, u32 w7, u32 len)
{
    void *m = 0;
    if (osal_valloc_msg_wait(0x00924126u, &m, len, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = 0x00924126u;
        p[0] = type; p[1] = w1; p[2] = w2; p[3] = w3;
        p[4] = w4; p[5] = w5; p[6] = w6; p[7] = w7;
        osal_snd_msg(0x00924126u, m);
    }
}

void __attribute__((constructor)) notify_all(void)
{
    /* GET-like 32-byte status blob (matches the sync GET reply). */
    send_msg(0x00920111u, 0, 4, 1, 0, 1, 0, 0, 0x20);
    /* ON completed C3P packet, 32-byte body. */
    send_msg(0x0092015bu, 0, 1, 0, 0, 0, 0, 0, 0x20);
    /* ON request packet. */
    send_msg(0x0092015au, 0, 1, 0, 0, 0, 0, 0, 0x20);
}
