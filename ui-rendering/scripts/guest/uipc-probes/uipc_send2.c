/* Test sender: deliver a synthetic context word to the GUI dynamic queue. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_msg(u32 qid, void *msg);

static void send_one(u32 qid, u32 word0)
{
    void *m = 0;
    s32 r = osal_valloc_msg_wait(qid, &m, 0x10, 1);
    if (r != 0 || m == 0) return;
    {
        u32 *p = (u32 *)m;
        p[-1] = qid;
        p[0] = word0;
        p[1] = 0;
        p[2] = 0;
        p[3] = 0;
    }
    osal_snd_msg(qid, m);
}

void __attribute__((constructor)) send_init(void)
{
    send_one(0x009460bf, 0x00015438);
}
