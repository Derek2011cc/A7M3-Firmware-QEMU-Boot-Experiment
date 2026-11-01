/* Deliver UPM_CMD_ON (0x92015a) asynchronously to im.elf's main loop queue. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_msg(u32 qid, void *msg);

void __attribute__((constructor)) main_on(void)
{
    void *m = 0;
    if (osal_valloc_msg_wait(0x0094016au, &m, 0x10, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = 0x0094016au;
        p[0] = 0x0092015au; p[1] = 0; p[2] = 0; p[3] = 0;
        osal_snd_msg(0x0094016au, m);
    }
}
