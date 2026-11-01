/* Probe UPM power-status field semantics one command at a time. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_sync_msg(u32 qid, void *msg);
extern s32 osal_free_msg(u32 qid, void *msg);

static void sync_cmd(u32 qid, u32 type, u32 p1)
{
    void *m = 0;
    if (osal_valloc_msg_wait(qid, &m, 0x20, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = qid;
        p[0] = type; p[1] = p1; p[2] = 0; p[3] = 0;
        osal_snd_sync_msg(qid, m);
        osal_free_msg(qid, m);
    }
}

void __attribute__((constructor)) upm_fields(void)
{
    u32 q = 0x00924126u;
    sync_cmd(q, 0x00920111u, 0);   /* GET initial */
    sync_cmd(q, 0x00920131u, 1); sync_cmd(q, 0x00920111u, 0);
    sync_cmd(q, 0x00920131u, 2); sync_cmd(q, 0x00920111u, 0);
    sync_cmd(q, 0x00920113u, 1); sync_cmd(q, 0x00920111u, 0);
    sync_cmd(q, 0x00920113u, 2); sync_cmd(q, 0x00920111u, 0);
    sync_cmd(q, 0x00920133u, 1); sync_cmd(q, 0x00920111u, 0);
    sync_cmd(q, 0x00920133u, 2); sync_cmd(q, 0x00920111u, 0);
    sync_cmd(q, 0x00920134u, 1); sync_cmd(q, 0x00920111u, 0);
    sync_cmd(q, 0x00920134u, 2); sync_cmd(q, 0x00920111u, 0);
    sync_cmd(q, 0x00920135u, 1); sync_cmd(q, 0x00920111u, 0);
    sync_cmd(q, 0x00920135u, 2); sync_cmd(q, 0x00920111u, 0);
}
