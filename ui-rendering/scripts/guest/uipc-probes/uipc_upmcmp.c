/* Deliver UPM_CMD_ON_CMP (0x0092015b) to the app's UPM notify queue. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_msg(u32 qid, void *msg);

static void send1(u32 qid, u32 t, u32 a, u32 b, u32 c)
{
    void *m = 0;
    if (osal_valloc_msg_wait(qid, &m, 0x20, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = qid; p[0] = t; p[1] = a; p[2] = b; p[3] = c;
        osal_snd_msg(qid, m);
    }
}

void __attribute__((constructor)) upm_cmp(void)
{
    send1(0x00924126, 0x0092015bu, 0, 1, 0);
}
