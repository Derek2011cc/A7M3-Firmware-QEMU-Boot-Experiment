/* Probe UPM C3P commands and let the preloaded shim log replies. */
typedef unsigned int u32;
typedef int s32;
extern s32 osal_valloc_msg_wait(u32 qid, void **msg, u32 size, u32 flags);
extern s32 osal_snd_sync_msg(u32 qid, void *msg);
extern s32 osal_free_msg(u32 qid, void *msg);

static void cmd(u32 type, u32 a, u32 b, u32 c)
{
    void *m = 0;
    if (osal_valloc_msg_wait(0x00924127, &m, 0x40, 1) == 0 && m) {
        u32 *p = (u32 *)m;
        p[-1] = 0x00924127;
        p[0] = type; p[1] = a; p[2] = b; p[3] = c;
        osal_snd_sync_msg(0x00924127, m);
        osal_free_msg(0x00924127, m);
    }
}

void __attribute__((constructor)) upm_probe(void)
{
    cmd(0x00920112u, 0, 0, 0);       /* RENOTIFY */
    cmd(0x00920111u, 0, 0, 0);       /* GET */
    cmd(0x00920111u, 0x403, 1, 0x395); /* GET with app-like params */
    cmd(0x00920169u, 0, 0, 0);       /* GET_PMCB_APP */
    cmd(0x0092016au, 0, 0, 0);       /* GET_PMCB_IM */
    cmd(0x0092016bu, 0, 0, 0);       /* GET_PMCB_LIRO */
    cmd(0x0092016du, 0, 0, 0);       /* CHECK_OFF_FACTOR */
    cmd(0x0092011cu, 0, 0, 0);       /* DUMP */
}
