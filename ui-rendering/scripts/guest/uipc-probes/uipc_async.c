/* Send one async message to the C3P sync-API queue 0x94016a. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_msg(u32 qid, void *msg);

void __attribute__((constructor)) send_async(void)
{
    void *m = 0;
    if (osal_valloc_msg_wait(0x0094016a, &m, 0x10, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = 0x0094016a;
        p[0] = 0x009400e1u;
        p[1] = 0; p[2] = 0; p[3] = 0;
        osal_snd_msg(0x0094016a, m);
    }
}
