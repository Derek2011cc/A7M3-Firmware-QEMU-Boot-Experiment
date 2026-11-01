/* Send a boot-style module-start command to the C3P service queue. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_msg(u32 qid, void *msg);

static void send_one(u32 qid, u32 type)
{
    void *m = 0;
    if (osal_valloc_msg_wait(qid, &m, 0x10, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = qid;
        p[0] = type;
        p[1] = 0;
        p[2] = 0;
        p[3] = 0;
        osal_snd_msg(qid, m);
    }
}

void __attribute__((constructor)) cmd_init(void)
{
    send_one(0x0094016a, 0x00940032u);
}
