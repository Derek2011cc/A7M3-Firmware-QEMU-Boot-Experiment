/* Drive UPM power-status fields to plausible "on" values via the UPM task. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_sync_msg(u32 qid, void *msg);
extern s32 osal_free_msg(u32 qid, void *msg);

static void send_sync(u32 qid, u32 type, u32 p1, u32 p2, u32 p3)
{
    void *m = 0;
    if (osal_valloc_msg_wait(qid, &m, 0x20, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = qid;
        p[0] = type; p[1] = p1; p[2] = p2; p[3] = p3;
        osal_snd_sync_msg(qid, m);
        osal_free_msg(qid, m);
    }
}

void __attribute__((constructor)) upm_set(void)
{
    u32 q = 0x00924126u;
    send_sync(q, 0x00920131u, 1, 0, 0);
    send_sync(q, 0x00920113u, 1, 0, 0);
    send_sync(q, 0x00920133u, 1, 0, 0);
    send_sync(q, 0x00920134u, 1, 0, 0);
    send_sync(q, 0x00920135u, 1, 0, 0);
    send_sync(q, 0x00920111u, 0, 0, 0);
    send_sync(q, 0x00920131u, 0x5343414eu, 0, 0);  /* trigger stack scan */
}
