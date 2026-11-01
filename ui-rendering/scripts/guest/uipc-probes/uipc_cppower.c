/* Simulate a CP -> AP power event: sync UPM_CMD_ON to im.elf's C3P API queue. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_sync_msg(u32 qid, void *msg);
extern s32 osal_free_msg(u32 qid, void *msg);

static void send_ssnd(u32 type, u32 a, u32 b, u32 c)
{
    void *m = 0;
    if (osal_valloc_msg_wait(0x0094016a, &m, 0x20, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = 0x0094016a;
        p[0] = type; p[1] = a; p[2] = b; p[3] = c;
        osal_snd_sync_msg(0x0094016a, m);
        osal_free_msg(0x0094016a, m);
    }
}

void __attribute__((constructor)) cp_power(void)
{
    send_ssnd(0x0092015au, 0, 0, 0);   /* UPM_CMD_ON */
}
