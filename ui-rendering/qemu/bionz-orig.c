/* QEMU model of Sony BIONZ image processors */

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "hw/block/flash.h"
#include "hw/boards.h"
#include "hw/char/pl011.h"
#include "hw/cpu/a9mpcore.h"
#include "hw/cpu/arm11mpcore.h"
#include "hw/hw.h"
#include "hw/irq.h"
#include "hw/loader.h"
#include "hw/sd/sdhci.h"
#include "qapi/error.h"
#include "sysemu/block-backend.h"
#include "sysemu/sysemu.h"
#include "target/arm/arm-tcm.h"

//////////////////////////// CXD4108 ////////////////////////////
#define CXD4108_NAND_BASE 0x00000000
#define CXD4108_DDR_BASE 0x20000000
#define CXD4108_DDR_SIZE 0x04000000
#define CXD4108_SDHCI_BASE 0x50000000
#define CXD4108_USB_BASE 0x70200000
#define CXD4108_DMA_BASE(i) (0x70500000 - (i) * 0x100000)
#define CXD4108_NUM_DMA 2
#define CXD4108_DMA_NUM_CHANNEL 8
#define CXD4108_PL320_BASE 0x70800000
#define CXD4108_UART_BASE(i) (0x74200000 + (i) * 0x100000)
#define CXD4108_NUM_UART 3
#define CXD4108_HWTIMER_BASE(i) (0x76000000 + (i) * 0x10000)
#define CXD4108_NUM_HWTIMER 8
#define CXD4108_SIO_BASE(i) (0x76100000 + (i) * 0x100000)
#define CXD4108_NUM_SIO 4
#define CXD4108_INTC_BASE(i) (0x76500000 + (i) * 0x100000)
#define CXD4108_SYSV_BASE 0x76700000
#define CXD4108_GPIO_BASE(i) (0x76710000 + (i) * 0x10000)
#define CXD4108_NUM_GPIO 6
#define CXD4108_GPIOEASY_BASE 0x76780000
#define CXD4108_GPIOSYS_BASE 0x76790000
#define CXD4108_MISCCTRL_BASE 0x767b0000
#define CXD4108_ADC_BASE(i) (0x76b00000 + (i + 1) * 0x100000)
#define CXD4108_NUM_ADC 2
#define CXD4108_CLKBLK_BASE 0x77400000
#define CXD4108_CAM_SYNC_BASE 0x78100000
#define CXD4108_SDC_BASE 0x78200000
#define CXD4108_JPEG_BASE 0x78c00000
#define CXD4108_RC_BASE(i) (0x79200000 + (i) * 0x100000)
#define CXD4108_NUM_RC 2
#define CXD4108_CPYFB_BASE 0x79700000
#define CXD4108_VIP_BASE 0x79800000
#define CXD4108_AUDIO_BASE 0x79900000
#define CXD4108_DDR_ALIAS_BASE 0xa0000000
#define CXD4108_BOOTROM_BASE 0xffff0000
#define CXD4108_BOOTROM_SIZE 0x00002000
#define CXD4108_SRAM_BASE 0xffff2000
#define CXD4108_SRAM_SIZE 0x00002000

#define CXD4108_IRQ_CH_UART 0
#define CXD4108_IRQ_CH_TIMER 2
#define CXD4108_IRQ_CH_DMA 3
#define CXD4108_IRQ_CH_SIO 7
#define CXD4108_IRQ_CH_SDHCI 8
#define CXD4108_IRQ_CH_USB 13
#define CXD4108_IRQ_CH_ADC 17
#define CXD4108_IRQ_CH_GPIO 19
#define CXD4108_IRQ_CH_PL320 21
#define CXD4108_IRQ_CH_VIDEO 23
#define CXD4108_IRQ_CH_IMGMC 27
#define CXD4108_IRQ_CH_IMGV 29
#define CXD4108_IRQ_CH_SYSV 30
#define CXD4108_IRQ_CH_AUDIO 31
#define CXD4108_IRQ_GPIO_NAND 15

#define CXD4108_TEXT_OFFSET 0x00408000
#define CXD4108_INITRD_OFFSET 0x0062e000

//////////////////////////// CXD4115 ////////////////////////////
#define CXD4115_NAND_BASE 0x00000000
#define CXD4115_DDR_BASE 0x10000000
#define CXD4115_DDR_SIZE 0x10000000
#define CXD4115_DMA_BASE 0x78008000
#define CXD4115_DMA_NUM_CHANNEL 8
#define CXD4115_USB_BASE 0x78020000
#define CXD4115_LDEC_BASE 0x78090000
#define CXD4115_ONA_BASE 0x78098000
#define CXD4115_HWTIMER_BASE(i) (0x7a000000 + (i) * 0x20)
#define CXD4115_NUM_HWTIMER 3
#define CXD4115_SIO_BASE(i) (0x7a008000 + (i) * 0x200)
#define CXD4115_NUM_SIO 5
#define CXD4115_UART_BASE(i) (0x7a050000 + (i) * 0x1000)
#define CXD4115_NUM_UART 3
#define CXD4115_GPIO_BASE(i) (0x7a400000 + (i) * 0x100)
#define CXD4115_NUM_GPIO 8
#define CXD4115_BOOTCON_BASE 0x7f000000
#define CXD4115_SRAM_BASE 0xfff00000
#define CXD4115_SRAM_SIZE 0x00008000
#define CXD4115_MPCORE_BASE 0xfffd0000
#define CXD4115_BOOTROM_BASE 0xffff0000
#define CXD4115_BOOTROM_SIZE 0x00002000

#define CXD4115_NUM_IRQ 256
#define CXD4115_IRQ_OFFSET 32
#define CXD4115_IRQ_GPIO_RISE(i) (32 + (i))
#define CXD4115_IRQ_GPIO_FALL(i) (112 + (i))
#define CXD4115_IRQ_UART(i) (152 + (i))
#define CXD4115_IRQ_HWTIMER(i) (155 + (i))
#define CXD4115_IRQ_DMA(i) (168 + (i))
#define CXD4115_IRQ_SIO(i) (188 + (i))
#define CXD4115_IRQ_USB0 233
#define CXD4115_IRQ_USB1 234
#define CXD4115_IRQ_GPIO_NAND 22

#define CXD4115_TYPEID_OFFSET 0x00007d24
#define CXD4115_TEXT_OFFSET 0x00208000
#define CXD4115_INITRD_OFFSET 0x0062e000

//////////////////////////// CXD4132 ////////////////////////////
#define CXD4132_NAND_BASE 0x00000000
#define CXD4132_DDR_BASE 0x80000000
#define CXD4132_DDR_SIZE 0x20000000
#define CXD4132_SRAM_BASE 0xa0000000
#define CXD4132_SRAM_SIZE 0x00400000
#define CXD4132_USB_BASE 0xf0040000
#define CXD4132_BOOTCON_BASE 0xf0100000
#define CXD4132_DMA_BASE 0xf2001000
#define CXD4132_DMA_NUM_CHANNEL 4
#define CXD4132_MENO_BASE 0xf2002000
#define CXD4132_HWTIMER_BASE(i) (0xf2008000 + (i) * 0x20)
#define CXD4132_NUM_HWTIMER 5
#define CXD4132_SIO_BASE(i) (0xf2010000 + (i) * 0x200)
#define CXD4132_NUM_SIO 5
#define CXD4132_UART_BASE(i) (0xf2038000 + (i) * 0x1000)
#define CXD4132_NUM_UART 3
#define CXD4132_GPIO_BASE(i) (0xf3000000 + (i) * 0x100)
#define CXD4132_NUM_GPIO 16
#define CXD4132_MISCCTRL_BASE(i) (0xf3060000 + (i) * 0x10)
#define CXD4132_MPCORE_BASE 0xf8000000
#define CXD4132_BOOTROM_BASE 0xffff0000
#define CXD4132_BOOTROM_SIZE 0x00006000

#define CXD4132_NUM_IRQ 256
#define CXD4132_IRQ_OFFSET 32
#define CXD4132_IRQ_UART(i) (160 + (i))
#define CXD4132_IRQ_HWTIMER(i) (163 + (i))
#define CXD4132_IRQ_DMA(i) (176 + (i))
#define CXD4132_IRQ_MENO 180
#define CXD4132_IRQ_NAND 183
#define CXD4132_IRQ_SIO(i) (196 + (i))
#define CXD4132_IRQ_USB 222

#define CXD4132_CMDLINE_OFFSET 0x00013000
#define CXD4132_TEXT_OFFSET 0x00018000
#define CXD4132_INITRD_OFFSET 0x00408000

#define CXD4132_CMDLINE \
    "lpj=622592 " \
    "console=ttyAM0,115200n8 " \
    "amba2.console=1 " \
    "ip=off " \
    "initrd=0x80408000,0x00700000 " \
    "root=/dev/ram0 " \
    "boottime=0x20000@0x833C0000 " \
    "klog.size=0x20000 " \
    "klog.addr=0x833E0080 " \
    "mem=64M@0x80000000@0 " \
    "memrsv=32K@0x80000000 " \
    "memrsv=0x1270000@0x82D90000 "

//////////////////////////// CXD90014 ////////////////////////////
#define CXD90014_BOSS_SRAM_BASE 0x00000000
#define CXD90014_BOSS_IO_BASE 0x00011000
#define CXD90014_NAND_REG_BASE 0x00020000
#define CXD90014_NAND_DATA_BASE 0x10000000
#define CXD90014_DDR_BASE 0x80000000
#define CXD90014_DDR_SIZE 0x40000000
#define CXD90014_SRAM_BASE 0xc0000000
#define CXD90014_SRAM_SIZE 0x01000000
#define CXD90014_BOOTCON_BASE 0xc0005030
#define CXD90014_DDMC_BASE 0xf0104000
#define CXD90014_USB_HDMAC_BASE 0xf0204000
#define CXD90014_USB_BASE 0xf0210000
#define CXD90014_UART_BASE(i) (0xf2000000 + (i) * 0x1000)
#define CXD90014_NUM_UART 3
#define CXD90014_HWTIMER_BASE(i) (0xf2403000 + (i) * 0x100)
#define CXD90014_NUM_HWTIMER 4
#define CXD90014_SIO_BASE(i) (0xf2405000 + (i) * 0x200)
#define CXD90014_NUM_SIO 5
#define CXD90014_BOSS_CLKRST_BASE 0xf29000d0
#define CXD90014_GPIO_BASE(i) (0xf2910000 + (i) * 0x100)
#define CXD90014_NUM_GPIO 18
#define CXD90014_MISCCTRL_BASE(i) (0xf2915000 + (i) * 0x10)
#define CXD90014_USB_OTG_BASE 0xf2920000
#define CXD90014_MPCORE_BASE 0xf8000000
#define CXD90014_BOOTROM_BASE 0xffff0000
#define CXD90014_BOOTROM_SIZE 0x00006000

#define CXD90014_NUM_IRQ 256
#define CXD90014_IRQ_OFFSET 32
#define CXD90014_IRQ_UART(i) (150 + (i))
#define CXD90014_IRQ_HWTIMER(i) (153 + (i))
#define CXD90014_IRQ_BOSS 170
#define CXD90014_IRQ_SIO(i) (201 + (i))
#define CXD90014_IRQ_USB 227

#define CXD90014_BOOT_BLOCK_OFFSET 0x00000000
#define CXD90014_BOOTROM_BLOCK_OFFSET 0x00000600
#define CXD90014_TEXT_OFFSET 0x00038000
#define CXD90014_INITRD_OFFSET 0x00628000

//////////////////////////// CXD90045 ////////////////////////////
/* A7M3 (CXD90045): the kernel is linked with PAGE_OFFSET 0x60000000 and
 * PHYS_OFFSET 0, so it is loaded at physical DDR0_BASE + TEXT_OFFSET and
 * its link addresses (0x60108000+) are virtual. */
#define CXD90045_DDR0_BASE 0x00000000
/* The 4.03 main kernel's real boot parameters live in nflasha2:/kemco.txt
 * (not the updater's nflasha1/BOOT/KEMCO.TXT):
 *   mem=1G@0, mem=256M@0x80000000@1,
 *   memrsv=704M@0x14000000:uc, memrsv=14M@0x80000000:uc,
 *   memrsv=242M@0x80e00000:uc, plus smaller reservations.
 * parse_memrsv() feeds the "uc" regions into Sony's noncache address table
 * at 0x604e07e0; without them arch_phys_to_noncache() rejects LIRO's
 * 0x8bdc1200 buffers (inside DDR1). */
#define CXD90045_DDR0_SIZE 0x40000000
/* DDR1: 256 MiB at 0x80000000 (main kernel mem=256M@0x80000000@1). */
#define CXD90045_DDR1_BASE 0x80000000
#define CXD90045_DDR1_SIZE 0x10000000
/* LIRO co-processor register window.  The firmware polls 0xf2a1102c bit1
 * (and later other status bits); QEMU does not model this block yet.  This
 * experimental region stores writes and can optionally return all-ones for
 * never-written registers to emulate "ready" status bits. */
#define CXD90045_LIRO_BASE 0xf2a00000
#define CXD90045_LIRO_SIZE 0x00023000
#define CXD90045_LIRO_NUM_REGS (CXD90045_LIRO_SIZE / 4)
/* Sugilite ("sugi") graphics engine BAR, seen in grm_gles.ko:
 * the driver ioremaps 0xf1000000 (size 0x4000) and userspace maps the same
 * page via /dev/mem.  The engine is kicked by writing (value|0x20000000) to
 * BAR+0xc0 relative to a per-engine base; completion is reported through the
 * BAR+0x34 status register and GIC IRQ 76.  A full rasteriser is out of
 * scope; this model completes every kick immediately so driver waits do not
 * hang, and traces register traffic for further reverse engineering. */
#define CXD90045_SUGI_BASE 0xf1000000
#define CXD90045_SUGI_SIZE 0x00004000
#define CXD90045_SUGI_NUM_REGS (CXD90045_SUGI_SIZE / 4)
#define CXD90045_SUGI_NUM_BLOCKS (CXD90045_SUGI_SIZE / 0x1000)
#define CXD90045_IRQ_SUGI 76
#define CXD90045_DDRC_BASE(i) (0xf0104000 + (i) * 0x1000)
#define CXD90045_SRAM_BASE 0xfe000000
#define CXD90045_SRAM_SIZE 0x01000000
#define CXD90045_BOOTCON_BASE 0xfe005030
#define CXD90045_SDHCI_BASE 0xf0304000
#define CXD90045_UART_BASE(i) (0xf2000000 + (i) * 0x1000)
#define CXD90045_NUM_UART 4
#define CXD90045_HWTIMER_BASE(i) (0xf2403000 + (i) * 0x100)
#define CXD90045_NUM_HWTIMER 4
#define CXD90045_SIO_BASE(i) (0xf2405000 + (i) * 0x200)
#define CXD90045_NUM_SIO 5
#define CXD90045_GPIO_BASE(i) (0xf2910000 + (i) * 0x100)
#define CXD90045_NUM_GPIO 18
#define CXD90045_MISCCTRL_BASE(i) (0xf2915000 + (i) * 0x10)
#define CXD90045_MPCORE_BASE 0xf8000000
#define CXD90045_BOOTROM_BASE 0xffff0000
#define CXD90045_BOOTROM_SIZE 0x00006000

#define CXD90045_NUM_IRQ 256
#define CXD90045_IRQ_OFFSET 32
#define CXD90045_IRQ_UART(i) ((i) == 3 ? 120 : (150 + (i)))
#define CXD90045_IRQ_HWTIMER(i) (153 + (i))
#define CXD90045_IRQ_SIO(i) (201 + (i))
#define CXD90045_IRQ_SDHCI 227

#define CXD90045_TEXT_OFFSET 0x00108000
#define CXD90045_INITRD_OFFSET 0x00700000
#define CXD90045_ATAGS_ADDR (CXD90045_DDR0_BASE + 0x00010000)
/* Memory-related options copied from the main kernel's real boot config
 * (eMMC image, around file offset 0x9bcad0).  The `mem=` lines are omitted
 * because the same banks are already supplied through ATAG_MEM. */
#define CXD90045_CMDLINE "console=ttyAM0,115200 root=/dev/ram0 rw ignore_loglevel earlyprintk initcall_debug ip=off log_buf_len=4M memrsv=1M@0 memrsv=0x8000@0xF00000 memrsv=0x1A5E000@0x02E68000 memrsv=0xA6EB000@0x09915000 memrsv=704M@0x14000000:uc memrsv=14M@0x80000000:uc memrsv=242M@0x80e00000:uc"

#define NAND_SECTOR_SIZE 0x200
#define NAND_PAGE_SIZE 0x1000

static hwaddr cxd_init_loader2(BlockBackend *drive)
{
    char boot_block[NAND_SECTOR_SIZE];
    uint32_t loader_offset, loader_size, loader_base;
    void *loader_buffer;

    if (blk_pread(drive, 0, boot_block, sizeof(boot_block)) < 0) {
        hw_error("%s: Cannot read boot block\n", __func__);
    }

    if (*(uint32_t *) boot_block != *(uint32_t *) "EXBL") {
        hw_error("%s: Wrong boot block signature\n", __func__);
    }

    loader_offset = (*(uint32_t *) (boot_block + 0x40)) * NAND_SECTOR_SIZE;
    loader_size = (*(uint32_t *) (boot_block + 0x44)) * NAND_SECTOR_SIZE;
    loader_base = *(uint32_t *) (boot_block + 0x50);

    loader_buffer = g_malloc(loader_size);
    if (blk_pread(drive, loader_offset, loader_buffer, loader_size) < 0) {
        hw_error("%s: Cannot read loader2\n", __func__);
    }
    rom_add_blob_fixed("loader2", loader_buffer, loader_size, loader_base);
    g_free(loader_buffer);

    return loader_base;
}

static hwaddr cxd90014_init_loader2(BlockBackend *drive)
{
    char page[0x600];
    uint32_t pages_per_block, block_offset, loader_offset, loader_n_pages, loader_base;
    int i;

    for (i = 0; i < 3; i++) {
        if (blk_pread(drive, i * NAND_PAGE_SIZE, page, sizeof(page)) < 0) {
            hw_error("%s: Cannot read bootrom block\n", __func__);
        }
        rom_add_blob_fixed("bootrom_block", page, sizeof(page), CXD90014_SRAM_BASE + CXD90014_BOOTROM_BLOCK_OFFSET + i * sizeof(page));
    }

    pages_per_block = 1 << (*(uint32_t *) (page + 0x08));
    block_offset = (*(uint32_t *) (page + 0x0c)) * pages_per_block * NAND_PAGE_SIZE;

    if (blk_pread(drive, block_offset, page, sizeof(page)) < 0) {
        hw_error("%s: Cannot read boot block\n", __func__);
    }

    if (*(uint32_t *) page != *(uint32_t *) "EXBL") {
        hw_error("%s: Wrong boot block signature\n", __func__);
    }

    rom_add_blob_fixed("boot_block", page, sizeof(page), CXD90014_SRAM_BASE + CXD90014_BOOT_BLOCK_OFFSET);

    loader_offset = block_offset + (*(uint32_t *) (page + 0x40)) * NAND_PAGE_SIZE;
    loader_n_pages = (*(uint32_t *) (page + 0x44));
    loader_base = *(uint32_t *) (page + 0x50);

    for (i = 0; i < loader_n_pages; i++) {
        if (blk_pread(drive, loader_offset + i * NAND_PAGE_SIZE, page, sizeof(page)) < 0) {
            hw_error("%s: Cannot read loader2\n", __func__);
        }
        rom_add_blob_fixed("loader2", page, sizeof(page), loader_base + i * sizeof(page));
    }

    return loader_base;
}

static hwaddr cxd90045_init_loader2(BlockBackend *drive)
{
    char boot_block[0x800];
    uint32_t boot_base, boot_size, loader_base;
    void *boot_buffer;

    if (blk_pread(drive, 0, boot_block, sizeof(boot_block)) < 0) {
        hw_error("%s: Cannot read boot block\n", __func__);
    }

    if (*(uint32_t *) boot_block != *(uint32_t *) "EXBL") {
        hw_error("%s: Wrong boot block signature\n", __func__);
    }

    boot_base = (*(uint32_t *) (boot_block + 0x6c));
    boot_size = (*(uint32_t *) (boot_block + 0x7c));
    loader_base = *(uint32_t *) (boot_block + 0x78);

    boot_buffer = g_malloc(boot_size);
    if (blk_pread(drive, 0, boot_buffer, boot_size) < 0) {
        hw_error("%s: Cannot read boot partition\n", __func__);
    }
    rom_add_blob_fixed("boot", boot_buffer, boot_size, boot_base);
    g_free(boot_buffer);

    return loader_base;
}

static void cxd_init_cmdline(const char *default_cmdline, const char *cmdline, hwaddr base)
{
    const char *header = "kemco ";
    const char *spacer = " ";
    const char *footer = " *";
    const size_t len = strlen(header) + strlen(default_cmdline) + strlen(spacer) + strlen(cmdline) + strlen(footer);
    char *buf = g_malloc(len + 1);
    strcpy(buf, header);
    strcat(buf, default_cmdline);
    strcat(buf, spacer);
    strcat(buf, cmdline);
    strcat(buf, footer);
    rom_add_blob_fixed("cmdline", buf, len, base);
    g_free(buf);
}

static void cxd_add_const_reg(const char *name, hwaddr base, uint32_t value)
{
    MemoryRegion *mem = g_new(MemoryRegion, 1);
    uint32_t *buffer = g_new(uint32_t, 1);
    *buffer = value;
    memory_region_init_ram_ptr(mem, NULL, name, sizeof(uint32_t), buffer);
    memory_region_set_readonly(mem, true);
    memory_region_add_subregion(get_system_memory(), base, mem);
}

static void cxd_write_bootloader(hwaddr base, hwaddr target)
{
    int i;
    uint32_t loader[] = {
        0xe51ff004, // ldr pc, [pc, #-4]
        target,
    };
    for (i = 0; i < ARRAY_SIZE(loader); i++) {
        loader[i] = tswap32(loader[i]);
    }
    rom_add_blob_fixed("bootloader", loader, sizeof(loader), base);
}

/* Local experiment: minimal ATAGS for the ARM32 Linux boot protocol. */
static void cxd90045_write_atags(uint32_t addr, uint32_t initrd_start,
                                 uint32_t initrd_size, const char *cmdline)
{
    uint32_t buffer[256];
    uint32_t *p = buffer;
    int len = strlen(cmdline) + 1;
    int words = (len + 3) / 4;

    /* ATAG layout: size first, then tag (struct tag_header). */
    *p++ = 5;         /* ATAG_CORE */
    *p++ = 0x54410001;
    *p++ = 0;
    *p++ = 4096;
    *p++ = 0;

    /* struct tag_mem32 { __u32 size; __u32 start; } */
    *p++ = 4;         /* ATAG_MEM: DDR0 at 0x60000000, 1 GiB */
    *p++ = 0x54410002;
    *p++ = CXD90045_DDR0_SIZE;
    *p++ = CXD90045_DDR0_BASE;

    *p++ = 4;         /* ATAG_MEM: DDR1, 256 MiB at 0x80000000 */
    *p++ = 0x54410002;
    *p++ = CXD90045_DDR1_SIZE;
    *p++ = CXD90045_DDR1_BASE;

    *p++ = 2 + words; /* ATAG_CMDLINE */
    *p++ = 0x54410009;
    memset(p, 0, words * 4);
    memcpy(p, cmdline, len);
    p += words;

    *p++ = 4;         /* ATAG_INITRD2 (0x54420005; 0x54410005 is deprecated ATAG_INITRD) */
    *p++ = 0x54420005;
    *p++ = initrd_start;
    *p++ = initrd_size;

    *p++ = 0;         /* ATAG_NONE */
    *p++ = 0;

    rom_add_blob_fixed("atags", buffer, (p - buffer) * 4, addr);
}

/* Local experiment: set r0/r1/r2 for the ARM32 Linux boot protocol. */
static void cxd_write_bootloader_with_machine(hwaddr base, hwaddr target,
                                              uint32_t machine, uint32_t atags)
{
    int i;
    uint32_t loader[] = {
        0xe3a00000, /* mov r0, #0 */
        0xe59f200c, /* ldr r2, [pc, #12] */
        0xe59f1004, /* ldr r1, [pc, #4] */
        0xe51ff004, /* ldr pc, [pc, #-4] */
        target,
        machine,
        atags,
    };
    for (i = 0; i < ARRAY_SIZE(loader); i++) {
        loader[i] = tswap32(loader[i]);
    }
    rom_add_blob_fixed("bootloader", loader, sizeof(loader), base);
}

static void cxd4108_init(MachineState *machine)
{
    DriveInfo *dinfo;
    BlockBackend *drive;
    MemoryRegion *mem, *ddr, *container;
    DeviceState *dev;
    Object *cpu;
    BusState *bus;
    qemu_irq irq[32][16];
    qemu_irq gpio_irq[16];
    qemu_irq vsync;
    int i, j, k;

    dinfo = drive_get(IF_MTD, 0, 0);
    drive = dinfo ? blk_by_legacy_dinfo(dinfo) : NULL;

    for (i = 0; i < machine->smp.cpus; i++) {
        cpu = object_new(machine->cpu_type);
        object_property_set_bool(cpu, "reset-hivecs", true, &error_fatal);
        if (i != 0) {
            object_property_set_bool(cpu, "start-powered-off", true, &error_fatal);

            container = g_new(MemoryRegion, 1);
            memory_region_init(container, NULL, "container", UINT64_MAX);
            mem = g_new(MemoryRegion, 1);
            memory_region_init_alias(mem, NULL, "sysmem", get_system_memory(), 0, UINT64_MAX);
            memory_region_add_subregion(container, 0, mem);
            object_property_set_link(cpu, "memory", OBJECT(container), &error_fatal);
            ARM_CPU(cpu)->tcmtr = 0x10001;
            arm_tcm_init(ARM_CPU(cpu), g_new(arm_tcm_mem, 1));
        }
        qdev_realize(DEVICE(cpu), NULL, &error_fatal);

        dev = qdev_new("bionz_intc");
        qdev_prop_set_uint32(dev, "len-enabled-channels", 1);
        qdev_prop_set_uint8(dev, "enabled-channels[0]", CXD4108_IRQ_CH_PL320);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_INTC_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ));
        for (j = 0; j < 32; j++) {
            for (k = 0; k < 16; k++) {
                if (i == 0) {
                    irq[j][k] = qdev_get_gpio_in(dev, j * 16 + k);
                } else {
                    irq[j][k] = qemu_irq_split(irq[j][k], qdev_get_gpio_in(dev, j * 16 + k));
                }
            }
        }
    }

    ddr = g_new(MemoryRegion, 1);
    memory_region_init_ram(ddr, NULL, "ddr", CXD4108_DDR_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4108_DDR_BASE, ddr);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_alias(mem, NULL, "ddr.alias", ddr, 0, CXD4108_DDR_SIZE);
    memory_region_add_subregion(get_system_memory(), CXD4108_DDR_ALIAS_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "sram", CXD4108_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4108_SRAM_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "bootrom", CXD4108_BOOTROM_SIZE, &error_fatal);
    memory_region_set_readonly(mem, true);
    memory_region_add_subregion(get_system_memory(), CXD4108_BOOTROM_BASE, mem);

    for (i = 0; i < CXD4108_NUM_GPIO; i++) {
        dev = qdev_new("bionz_gpio");
        qdev_prop_set_uint8(dev, "version", 1);
        qdev_prop_set_uint8(dev, "num-gpio", 16);
        dev->id = g_strdup_printf("gpio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_GPIO_BASE(i));
    }

    dev = qdev_new("bionz_gpio");
    qdev_prop_set_uint8(dev, "version", 1);
    qdev_prop_set_uint8(dev, "num-gpio", 16);
    dev->id = "gpioe";
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_GPIOEASY_BASE);

    dev = qdev_new("bionz_gpiosys");
    dev->id = "gpios";
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_GPIOSYS_BASE);
    for (i = 0; i < 16; i++) {
        gpio_irq[i] = qdev_get_gpio_in(dev, i);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, irq[CXD4108_IRQ_CH_GPIO][i]);
    }

    dev = qdev_new("onenand");
    qdev_prop_set_int32(dev, "shift", 1);
    qdev_prop_set_drive(dev, "drive", drive);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_NAND_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, gpio_irq[CXD4108_IRQ_GPIO_NAND]);

    dev = qdev_new(TYPE_SYSBUS_SDHCI);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_SDHCI_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_SDHCI][0]);
    bus = qdev_get_child_bus(dev, "sd-bus");

    dinfo = drive_get(IF_MTD, 0, 1);
    dev = qdev_new(TYPE_SD_CARD);
    qdev_prop_set_bit(dev, "emmc", true);
    qdev_prop_set_drive(dev, "drive", dinfo ? blk_by_legacy_dinfo(dinfo) : NULL);
    qdev_realize_and_unref(dev, bus, &error_fatal);

    dev = qdev_new("inventra_usb");
    qdev_prop_set_bit(dev, "dynfifo", true);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_USB_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_USB][0]);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1, irq[CXD4108_IRQ_CH_USB][1]);

    for (i = 0; i < CXD4108_NUM_DMA; i++) {
        dev = qdev_new("bionz_dma");
        qdev_prop_set_uint32(dev, "version", 1);
        qdev_prop_set_uint32(dev, "num-channel", CXD4108_DMA_NUM_CHANNEL);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_DMA_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), CXD4108_DMA_NUM_CHANNEL, irq[CXD4108_IRQ_CH_DMA][i]);
    }

    dev = qdev_new("pl320");
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_PL320_BASE);
    for (i = 0; i < 2; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, irq[CXD4108_IRQ_CH_PL320][i]);
    }

    for (i = 0; i < CXD4108_NUM_UART; i++) {
        pl011_create(CXD4108_UART_BASE(i), irq[CXD4108_IRQ_CH_UART][i], serial_hd(i));
    }

    for (i = 0; i < CXD4108_NUM_HWTIMER; i++) {
        dev = qdev_new("bionz_hwtimer");
        qdev_prop_set_uint32(dev, "freq", 2e6);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_HWTIMER_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_TIMER][i]);
    }

    for (i = 0; i < CXD4108_NUM_SIO; i++) {
        dev = qdev_new("bionz_sio");
        dev->id = g_strdup_printf("sio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_SIO_BASE(i));
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD4108_SIO_BASE(i) + 0x80000);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_SIO][i]);
    }

    for (i = 0; i < CXD4108_NUM_ADC; i++) {
        dev = qdev_new("bionz_adc");
        dev->id = g_strdup_printf("adc%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_ADC_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_ADC][i]);
    }

    dev = qdev_new("arm_power");
    qdev_prop_set_uint64(dev, "cpuid", 1);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_CLKBLK_BASE + 0x60);

    dev = qdev_new("bionz_cam_sync");
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_CAM_SYNC_BASE);

    dev = qdev_new("bionz_jpeg");
    qdev_prop_set_uint32(dev, "base", CXD4108_DDR_BASE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_JPEG_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD4108_JPEG_BASE + 0x800);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_IMGMC][5]);

    for (i = 0; i < CXD4108_NUM_RC; i++) {
        dev = qdev_new("bionz_rc");
        qdev_prop_set_uint32(dev, "base", CXD4108_DDR_BASE);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_RC_BASE(i) + 0x1000);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD4108_RC_BASE(i) + 0x2000);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_IMGMC][3 + i]);
    }

    dev = qdev_new("bionz_cpyfb");
    qdev_prop_set_uint32(dev, "base", CXD4108_DDR_BASE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_CPYFB_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD4108_CPYFB_BASE + 0x80000);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_VIDEO][4]);

    dev = qdev_new("bionz_vip");
    object_property_set_link(OBJECT(dev), "memory", OBJECT(ddr), &error_fatal);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_VIP_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD4108_VIP_BASE + 0x800);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_VIDEO][0]);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1, irq[CXD4108_IRQ_CH_VIDEO][1]);
    vsync = qdev_get_gpio_in(dev, 0);

    dev = qdev_new("bionz_sysv");
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_SYSV_BASE);
    for (i = 0; i < 10; i++) {
        if (i < 3) {
            sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, irq[CXD4108_IRQ_CH_IMGV][i]);
        } else {
            sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, irq[CXD4108_IRQ_CH_SYSV][i - 3]);
        }
    }
    qdev_connect_gpio_out(dev, 0, vsync);

    dev = qdev_new("bionz_audio");
    qdev_prop_set_uint32(dev, "base", CXD4108_DDR_BASE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4108_AUDIO_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD4108_AUDIO_BASE + 0x1000);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4108_IRQ_CH_AUDIO][0]);

    if (machine->kernel_filename) {
        load_image_targphys(machine->kernel_filename, CXD4108_DDR_BASE + CXD4108_TEXT_OFFSET, CXD4108_DDR_SIZE - CXD4108_TEXT_OFFSET);
        load_image_targphys(machine->initrd_filename, CXD4108_DDR_BASE + CXD4108_INITRD_OFFSET, CXD4108_DDR_SIZE - CXD4108_INITRD_OFFSET);
        cxd_write_bootloader(CXD4108_BOOTROM_BASE, CXD4108_DDR_BASE + CXD4108_TEXT_OFFSET);
    } else if (bios_name) {
        load_image_targphys(bios_name, CXD4108_BOOTROM_BASE, CXD4108_BOOTROM_SIZE);
    } else if (drive) {
        cxd_write_bootloader(CXD4108_BOOTROM_BASE, cxd_init_loader2(drive));
    }

    cxd_add_const_reg("miscctrl_mode", CXD4108_MISCCTRL_BASE, 0x101);

    cxd_add_const_reg("sdc_para4", CXD4108_SDC_BASE + 0xc, 0x80000000);
}

static void cxd4115_init(MachineState *machine)
{
    DriveInfo *dinfo;
    BlockBackend *drive;
    MemoryRegion *mem;
    DeviceState *dev;
    Object *cpu;
    qemu_irq irq[CXD4115_NUM_IRQ - CXD4115_IRQ_OFFSET];
    qemu_irq gpio_irq[24];
    int i, j, k;

    dinfo = drive_get(IF_MTD, 0, 0);
    drive = dinfo ? blk_by_legacy_dinfo(dinfo) : NULL;

    cpu = object_new(machine->cpu_type);
    object_property_set_bool(cpu, "reset-hivecs", true, &error_fatal);
    qdev_realize(DEVICE(cpu), NULL, &error_fatal);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "ddr", CXD4115_DDR_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4115_DDR_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "sram", CXD4115_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4115_SRAM_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "bootrom", CXD4115_BOOTROM_SIZE, &error_fatal);
    memory_region_set_readonly(mem, true);
    memory_region_add_subregion(get_system_memory(), CXD4115_BOOTROM_BASE, mem);

    dev = qdev_new(TYPE_ARM11MPCORE_PRIV);
    qdev_prop_set_uint32(dev, "num-cpu", 1);
    qdev_prop_set_uint32(dev, "num-irq", CXD4115_NUM_IRQ);
    qdev_prop_set_uint32(DEVICE(&ARM11MPCORE_PRIV(dev)->mptimer), "freq", 156e6);
    qdev_prop_set_uint32(DEVICE(&ARM11MPCORE_PRIV(dev)->wdtimer), "freq", 156e6);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_MPCORE_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ));
    for (i = 0; i < CXD4115_NUM_IRQ - CXD4115_IRQ_OFFSET; i++) {
        irq[i] = qdev_get_gpio_in(dev, i);
    }

    for (i = 0; i < CXD4115_NUM_GPIO; i++) {
        dev = qdev_new("bionz_gpio");
        qdev_prop_set_uint8(dev, "version", 1);
        dev->id = g_strdup_printf("gpio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_GPIO_BASE(i));
        if (i == 0) {
            for (j = 0; j < 20; j++) {
                k = (j < 16) ? j : j + 4;
                gpio_irq[k] = qdev_get_gpio_in(dev, j);
                sysbus_connect_irq(SYS_BUS_DEVICE(dev), j, qemu_irq_split(
                    irq[CXD4115_IRQ_GPIO_RISE(k) - CXD4115_IRQ_OFFSET],
                    qemu_irq_invert(irq[CXD4115_IRQ_GPIO_FALL(k) - CXD4115_IRQ_OFFSET])
                ));
            }
        }
    }

    dev = qdev_new("onenand");
    qdev_prop_set_uint16(dev, "manufacturer_id", NAND_MFR_SAMSUNG);
    qdev_prop_set_int32(dev, "shift", 1);
    qdev_prop_set_drive(dev, "drive", drive);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_NAND_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, gpio_irq[CXD4115_IRQ_GPIO_NAND]);

    dev = qdev_new("bionz_dma");
    qdev_prop_set_uint32(dev, "version", 1);
    qdev_prop_set_uint32(dev, "num-channel", CXD4115_DMA_NUM_CHANNEL);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_DMA_BASE);
    for (i = 0; i < CXD4115_DMA_NUM_CHANNEL; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, irq[CXD4115_IRQ_DMA(i) - CXD4115_IRQ_OFFSET]);
    }

    dev = qdev_new("inventra_usb");
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_USB_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4115_IRQ_USB0 - CXD4115_IRQ_OFFSET]);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1, irq[CXD4115_IRQ_USB1 - CXD4115_IRQ_OFFSET]);

    dev = qdev_new("bionz_ldec");
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_LDEC_BASE);

    for (i = 0; i < CXD4115_NUM_HWTIMER; i++) {
        dev = qdev_new("bionz_hwtimer");
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_HWTIMER_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4115_IRQ_HWTIMER(i) - CXD4115_IRQ_OFFSET]);
    }

    for (i = 0; i < CXD4115_NUM_SIO; i++) {
        dev = qdev_new("bionz_sio");
        dev->id = g_strdup_printf("sio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_SIO_BASE(i));
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD4115_SIO_BASE(i) + 0x100);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4115_IRQ_SIO(i) - CXD4115_IRQ_OFFSET]);
    }

    for (i = 0; i < CXD4115_NUM_UART; i++) {
        pl011_create(CXD4115_UART_BASE(i), irq[CXD4115_IRQ_UART(i) - CXD4115_IRQ_OFFSET], serial_hd(i));
    }

    dev = qdev_new("bionz_bootcon");
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_BOOTCON_BASE);

    if (machine->kernel_filename) {
        load_image_targphys(machine->kernel_filename, CXD4115_DDR_BASE + CXD4115_TEXT_OFFSET, CXD4115_DDR_SIZE - CXD4115_TEXT_OFFSET);
        load_image_targphys(machine->initrd_filename, CXD4115_DDR_BASE + CXD4115_INITRD_OFFSET, CXD4115_DDR_SIZE - CXD4115_INITRD_OFFSET);
        cxd_write_bootloader(CXD4115_BOOTROM_BASE, CXD4115_DDR_BASE + CXD4115_TEXT_OFFSET);
    } else if (bios_name) {
        load_image_targphys(bios_name, CXD4115_BOOTROM_BASE, CXD4115_BOOTROM_SIZE);
    } else if (drive) {
        cxd_write_bootloader(CXD4115_BOOTROM_BASE, cxd_init_loader2(drive));
        uint32_t typeid = 1;
        rom_add_blob_fixed("typeid", &typeid, sizeof(typeid), CXD4115_SRAM_BASE + CXD4115_TYPEID_OFFSET);
    }

    cxd_add_const_reg("ona_reset", CXD4115_ONA_BASE, 1);
}

static void cxd4132_init(MachineState *machine)
{
    DriveInfo *dinfo;
    BlockBackend *drive;
    MemoryRegion *mem;
    DeviceState *dev;
    Object *cpu;
    qemu_irq irq[CXD4132_NUM_IRQ - CXD4132_IRQ_OFFSET];
    int i;

    dinfo = drive_get(IF_MTD, 0, 0);
    drive = dinfo ? blk_by_legacy_dinfo(dinfo) : NULL;

    cpu = object_new(machine->cpu_type);
    object_property_set_bool(cpu, "reset-hivecs", true, &error_fatal);
    qdev_realize(DEVICE(cpu), NULL, &error_fatal);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "ddr", CXD4132_DDR_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4132_DDR_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "sram", CXD4132_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4132_SRAM_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "bootrom", CXD4132_BOOTROM_SIZE, &error_fatal);
    memory_region_set_readonly(mem, true);
    memory_region_add_subregion(get_system_memory(), CXD4132_BOOTROM_BASE, mem);

    dev = qdev_new(TYPE_ARM11MPCORE_PRIV);
    qdev_prop_set_uint32(dev, "num-cpu", 1);
    qdev_prop_set_uint32(dev, "num-irq", CXD4132_NUM_IRQ);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_MPCORE_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ));
    for (i = 0; i < CXD4132_NUM_IRQ - CXD4132_IRQ_OFFSET; i++) {
        irq[i] = qdev_get_gpio_in(dev, i);
    }

    for (i = 0; i < CXD4132_NUM_GPIO; i++) {
        dev = qdev_new("bionz_gpio");
        qdev_prop_set_uint8(dev, "version", 2);
        dev->id = g_strdup_printf("gpio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_GPIO_BASE(i));
    }

    dev = qdev_new("onenand");
    qdev_prop_set_uint16(dev, "manufacturer_id", NAND_MFR_SAMSUNG);
    qdev_prop_set_int32(dev, "shift", 1);
    qdev_prop_set_drive(dev, "drive", drive);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_NAND_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4132_IRQ_NAND - CXD4132_IRQ_OFFSET]);

    dev = qdev_new("synopsys_usb");
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_USB_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4132_IRQ_USB - CXD4132_IRQ_OFFSET]);

    dev = qdev_new("bionz_bootcon");
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_BOOTCON_BASE);

    dev = qdev_new("bionz_dma");
    qdev_prop_set_uint32(dev, "version", 2);
    qdev_prop_set_uint32(dev, "num-channel", CXD4132_DMA_NUM_CHANNEL);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_DMA_BASE);
    for (i = 0; i < CXD4132_DMA_NUM_CHANNEL; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, irq[CXD4132_IRQ_DMA(i) - CXD4132_IRQ_OFFSET]);
    }

    dev = qdev_new("bionz_meno");
    if (drive) {
        qdev_prop_set_string(dev, "drive_name", blk_name(drive));
    }
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_MENO_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4132_IRQ_MENO - CXD4132_IRQ_OFFSET]);

    for (i = 0; i < CXD4132_NUM_HWTIMER; i++) {
        dev = qdev_new("bionz_hwtimer");
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_HWTIMER_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4132_IRQ_HWTIMER(i) - CXD4132_IRQ_OFFSET]);
    }

    for (i = 0; i < CXD4132_NUM_SIO; i++) {
        dev = qdev_new("bionz_sio");
        dev->id = g_strdup_printf("sio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_SIO_BASE(i));
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD4132_SIO_BASE(i) + 0x100);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4132_IRQ_SIO(i) - CXD4132_IRQ_OFFSET]);
    }

    for (i = 0; i < CXD4132_NUM_UART; i++) {
        pl011_create(CXD4132_UART_BASE(i), irq[CXD4132_IRQ_UART(i) - CXD4132_IRQ_OFFSET], serial_hd(i));
    }

    if (machine->kernel_filename) {
        load_image_targphys(machine->kernel_filename, CXD4132_DDR_BASE + CXD4132_TEXT_OFFSET, CXD4132_DDR_SIZE - CXD4132_TEXT_OFFSET);
        load_image_targphys(machine->initrd_filename, CXD4132_DDR_BASE + CXD4132_INITRD_OFFSET, CXD4132_DDR_SIZE - CXD4132_INITRD_OFFSET);
        cxd_init_cmdline(CXD4132_CMDLINE, machine->kernel_cmdline, CXD4132_DDR_BASE + CXD4132_CMDLINE_OFFSET);
        cxd_write_bootloader(CXD4132_BOOTROM_BASE, CXD4132_DDR_BASE + CXD4132_TEXT_OFFSET);
    } else if (bios_name) {
        load_image_targphys(bios_name, CXD4132_BOOTROM_BASE, CXD4132_BOOTROM_SIZE);
    } else if (drive) {
        cxd_write_bootloader(CXD4132_BOOTROM_BASE, cxd_init_loader2(drive));
    }

    cxd_add_const_reg("miscctrl_readdone", CXD4132_MISCCTRL_BASE(1), 1);
    cxd_add_const_reg("miscctrl_typeid", CXD4132_MISCCTRL_BASE(2), 0x301);
}

static void cxd90014_init(MachineState *machine)
{
    DriveInfo *dinfo;
    BlockBackend *drive;
    MemoryRegion *mem;
    DeviceState *dev;
    Object *cpu;
    qemu_irq irq[CXD90014_NUM_IRQ - CXD90014_IRQ_OFFSET];
    qemu_irq boss_irq;
    int i;

    dinfo = drive_get(IF_MTD, 0, 0);
    drive = dinfo ? blk_by_legacy_dinfo(dinfo) : NULL;

    cpu = object_new(machine->cpu_type);
    object_property_set_bool(cpu, "has_el3", false, &error_fatal);
    object_property_set_bool(cpu, "reset-hivecs", true, &error_fatal);
    qdev_realize(DEVICE(cpu), NULL, &error_fatal);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "ddr", CXD90014_DDR_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD90014_DDR_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "sram", CXD90014_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD90014_SRAM_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "bootrom", CXD90014_BOOTROM_SIZE, &error_fatal);
    memory_region_set_readonly(mem, true);
    memory_region_add_subregion(get_system_memory(), CXD90014_BOOTROM_BASE, mem);

    dev = qdev_new(TYPE_A9MPCORE_PRIV);
    qdev_prop_set_uint32(dev, "num-cpu", 1);
    qdev_prop_set_uint32(dev, "num-irq", CXD90014_NUM_IRQ);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_MPCORE_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ));
    for (i = 0; i < CXD90014_NUM_IRQ - CXD90014_IRQ_OFFSET; i++) {
        irq[i] = qdev_get_gpio_in(dev, i);
    }

    dev = qdev_new("bionz_boss");
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_BOSS_SRAM_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD90014_BOSS_IO_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 2, CXD90014_BOSS_CLKRST_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90014_IRQ_BOSS - CXD90014_IRQ_OFFSET]);
    boss_irq = qdev_get_gpio_in(dev, 0);

    for (i = 0; i < CXD90014_NUM_GPIO; i++) {
        dev = qdev_new("bionz_gpio");
        qdev_prop_set_uint8(dev, "version", 3);
        dev->id = g_strdup_printf("gpio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_GPIO_BASE(i));
    }

    dev = qdev_new("bionz_nand");
    qdev_prop_set_drive(dev, "drive", drive);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_NAND_REG_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD90014_NAND_DATA_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, boss_irq);

    dev = qdev_new("bionz_bootcon");
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_BOOTCON_BASE);

    dev = qdev_new("fujitsu_usb");
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_USB_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD90014_USB_HDMAC_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90014_IRQ_USB - CXD90014_IRQ_OFFSET]);

    for (i = 0; i < CXD90014_NUM_UART; i++) {
        pl011_create(CXD90014_UART_BASE(i), irq[CXD90014_IRQ_UART(i) - CXD90014_IRQ_OFFSET], serial_hd(i));
    }

    for (i = 0; i < CXD90014_NUM_HWTIMER; i++) {
        dev = qdev_new("bionz_hwtimer");
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_HWTIMER_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90014_IRQ_HWTIMER(i) - CXD90014_IRQ_OFFSET]);
    }

    for (i = 0; i < CXD90014_NUM_SIO; i++) {
        dev = qdev_new("bionz_sio");
        dev->id = g_strdup_printf("sio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_SIO_BASE(i));
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD90014_SIO_BASE(i) + 0x100);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90014_IRQ_SIO(i) - CXD90014_IRQ_OFFSET]);
    }

    if (machine->kernel_filename) {
        load_image_targphys(machine->kernel_filename, CXD90014_DDR_BASE + CXD90014_TEXT_OFFSET, CXD90014_DDR_SIZE - CXD90014_TEXT_OFFSET);
        load_image_targphys(machine->initrd_filename, CXD90014_DDR_BASE + CXD90014_INITRD_OFFSET, CXD90014_DDR_SIZE - CXD90014_INITRD_OFFSET);
        cxd_write_bootloader(CXD90014_BOOTROM_BASE, CXD90014_DDR_BASE + CXD90014_TEXT_OFFSET);
    } else if (bios_name) {
        load_image_targphys(bios_name, CXD90014_BOOTROM_BASE, CXD90014_BOOTROM_SIZE);
    } else if (drive) {
        cxd_write_bootloader(CXD90014_BOOTROM_BASE, cxd90014_init_loader2(drive));
    }

    cxd_add_const_reg("miscctrl_typeid", CXD90014_MISCCTRL_BASE(0), 0x500);
    cxd_add_const_reg("miscctrl_mode", CXD90014_MISCCTRL_BASE(1), 0x0c010003);

    cxd_add_const_reg("ddmc_ctl_int_status", CXD90014_DDMC_BASE + 0x128, 0x10);

    cxd_add_const_reg("fusb_otg_usb_id_ext", CXD90014_USB_OTG_BASE + 0x10, 2);
}

/* Experimental LIRO register window: see docs/LIRO-AND-MEMORY.md.
 * Writes are stored; reads of never-written registers return zero, or
 * all-ones when A7M3_LIRO_AUTOREADY is set in the environment. */
typedef struct CXD90045LiroState {
    uint32_t regs[CXD90045_LIRO_NUM_REGS];
    uint8_t written[CXD90045_LIRO_NUM_REGS];
    unsigned trace_count;
} CXD90045LiroState;

static bool cxd90045_liro_trace(void)
{
    static int enabled = -1;
    if (enabled < 0) {
        enabled = getenv("A7M3_LIRO_TRACE") != NULL;
    }
    return enabled;
}

static bool cxd90045_liro_autoready(void)
{
    static int enabled = -1;
    if (enabled < 0) {
        enabled = getenv("A7M3_LIRO_AUTOREADY") != NULL;
    }
    return enabled;
}

static bool cxd90045_liro_status_reg(uint32_t offset)
{
    uint32_t low = offset & 0xfff;
    return low == 0x004 || low == 0x02c;
}

static uint64_t cxd90045_liro_read(void *opaque, hwaddr offset, unsigned size)
{
    CXD90045LiroState *s = opaque;
    uint32_t idx = offset >> 2;
    uint32_t value;
    uint32_t pc = 0, lr = 0;

    if (idx >= CXD90045_LIRO_NUM_REGS) {
        return 0;
    }
    if (s->written[idx]) {
        value = s->regs[idx];
    } else if (cxd90045_liro_autoready() &&
               cxd90045_liro_status_reg((uint32_t)offset)) {
        value = 0xffffffffu;
    } else {
        value = 0;
    }
    if (current_cpu) {
        pc = ARM_CPU(current_cpu)->env.regs[15];
        lr = ARM_CPU(current_cpu)->env.regs[14];
    }
    if (cxd90045_liro_trace() && s->trace_count < 4000 &&
        pc >= 0x635c0000 && pc < 0x65000000) {
        fprintf(stderr,
                "A7M3-LIRO RD[%u] pc=%08x lr=%08x off=%04x val=%08x size=%u\n",
                s->trace_count, pc, lr, (unsigned)offset, value, size);
        s->trace_count++;
    }
    return value;
}

static void cxd90045_liro_write(void *opaque, hwaddr offset, uint64_t value,
                                unsigned size)
{
    CXD90045LiroState *s = opaque;
    uint32_t idx = offset >> 2;
    uint32_t pc = 0, lr = 0;

    if (idx >= CXD90045_LIRO_NUM_REGS) {
        return;
    }
    /* The register at instance offset 0x030 is a "start and self-clear"
     * bit: firmware writes 1, then polls until the register reads 0. */
    if ((offset & 0xfff) == 0x030) {
        s->regs[idx] = 0;
    } else {
        s->regs[idx] = (uint32_t)value;
    }
    s->written[idx] = 1;
    if (current_cpu) {
        pc = ARM_CPU(current_cpu)->env.regs[15];
        lr = ARM_CPU(current_cpu)->env.regs[14];
    }
    if (cxd90045_liro_trace() && s->trace_count < 4000 &&
        pc >= 0x635c0000 && pc < 0x65000000) {
        fprintf(stderr,
                "A7M3-LIRO WR[%u] pc=%08x lr=%08x off=%04x val=%08x size=%u\n",
                s->trace_count, pc, lr, (unsigned)offset, (unsigned)value, size);
        s->trace_count++;
    }
}

static const MemoryRegionOps cxd90045_liro_ops = {
    .read = cxd90045_liro_read,
    .write = cxd90045_liro_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/* Minimal Sugilite graphics engine model: stores registers, completes every
 * kick by raising IRQ 76 with a synthetic status value, and clears pending
 * state when the driver writes the ack registers observed in grm_gles.ko. */
#define TYPE_CXD90045_SUGI "cxd90045_sugi"
#define CXD90045_SUGI(obj) \
    OBJECT_CHECK(CXD90045SugiState, (obj), TYPE_CXD90045_SUGI)

typedef struct CXD90045SugiState {
    SysBusDevice parent_obj;
    MemoryRegion mmio;
    qemu_irq irq;
    uint32_t regs[CXD90045_SUGI_NUM_REGS];
    uint32_t status[CXD90045_SUGI_NUM_BLOCKS];
    uint8_t pending[CXD90045_SUGI_NUM_BLOCKS];
    uint32_t irq_status;
    unsigned trace_count;
} CXD90045SugiState;

static bool cxd90045_sugi_trace(void)
{
    static int enabled = -1;
    if (enabled < 0) {
        enabled = getenv("A7M3_SUGI_TRACE") != NULL;
    }
    return enabled;
}

static bool cxd90045_sugi_ack_off(uint32_t low)
{
    return low == 0xc18 || low == 0x01c || low == 0x03c || low == 0x044;
}

static uint64_t cxd90045_sugi_read(void *opaque, hwaddr offset, unsigned size)
{
    CXD90045SugiState *s = opaque;
    uint32_t idx = offset >> 2;
    uint32_t low = offset & 0xfff;
    uint32_t val;

    if (idx >= CXD90045_SUGI_NUM_REGS) {
        return 0;
    }
    val = s->regs[idx];
    if (low == 0x034 && s->irq_status) {
        val = s->irq_status;
    }
    if (cxd90045_sugi_trace() && s->trace_count < 4000) {
        uint32_t pc = 0, lr = 0;
        CPUState *cpu = current_cpu;
        if (cpu) {
            pc = ARM_CPU(cpu)->env.regs[15];
            lr = ARM_CPU(cpu)->env.regs[14];
        }
        fprintf(stderr, "A7M3-SUGI RD[%u] pc=%08x lr=%08x off=%04x val=%08x "
                "size=%u\n", s->trace_count, pc, lr, (unsigned)offset, val,
                size);
        s->trace_count++;
    }
    return val;
}

static void cxd90045_sugi_write(void *opaque, hwaddr offset, uint64_t value,
                                unsigned size)
{
    CXD90045SugiState *s = opaque;
    uint32_t idx = offset >> 2;
    uint32_t low = offset & 0xfff;
    uint32_t block = (offset >> 12) & (CXD90045_SUGI_NUM_BLOCKS - 1);

    if (idx >= CXD90045_SUGI_NUM_REGS) {
        return;
    }
    s->regs[idx] = (uint32_t)value;

    if ((offset == 0x00c0 && (value & 0x20000000u)) ||
        (offset == 0x1800 && (value & 0x4u)) ||
        (offset == 0x0c18 && value == 1)) {
        /* Kick: complete immediately.  The driver reads BAR+0x34 and acks
         * bit30, bit26 and the low nibble.  The status register the handler
         * reads is fixed at block 0, so publish every completion there. */
        s->status[block] = 0xc0000001;
        s->pending[block] = 1;
        s->irq_status |= s->status[block];
        qemu_irq_raise(s->irq);
    } else if (cxd90045_sugi_ack_off(low)) {
        s->pending[block] = 0;
        s->irq_status = 0;
        qemu_irq_lower(s->irq);
    }

    if (cxd90045_sugi_trace() && s->trace_count < 4000) {
        uint32_t pc = 0, lr = 0;
        CPUState *cpu = current_cpu;
        if (cpu) {
            pc = ARM_CPU(cpu)->env.regs[15];
            lr = ARM_CPU(cpu)->env.regs[14];
        }
        fprintf(stderr, "A7M3-SUGI WR[%u] pc=%08x lr=%08x off=%04x val=%08x "
                "size=%u\n", s->trace_count, pc, lr, (unsigned)offset,
                (unsigned)value, size);
        s->trace_count++;
    }
}

static const MemoryRegionOps cxd90045_sugi_ops = {
    .read = cxd90045_sugi_read,
    .write = cxd90045_sugi_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void cxd90045_sugi_reset(DeviceState *dev)
{
    CXD90045SugiState *s = CXD90045_SUGI(dev);
    memset(s->regs, 0, sizeof(s->regs));
    memset(s->status, 0, sizeof(s->status));
    memset(s->pending, 0, sizeof(s->pending));
    s->irq_status = 0;
}

static void cxd90045_sugi_realize(DeviceState *dev, Error **errp)
{
    CXD90045SugiState *s = CXD90045_SUGI(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &cxd90045_sugi_ops, s,
                          TYPE_CXD90045_SUGI, CXD90045_SUGI_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
}

static void cxd90045_sugi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = cxd90045_sugi_realize;
    dc->reset = cxd90045_sugi_reset;
}

static const TypeInfo cxd90045_sugi_info = {
    .name = TYPE_CXD90045_SUGI,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CXD90045SugiState),
    .class_init = cxd90045_sugi_class_init,
};

static void cxd90045_sugi_register_types(void)
{
    type_register_static(&cxd90045_sugi_info);
}

type_init(cxd90045_sugi_register_types)

static void cxd90045_init(MachineState *machine)
{
    DriveInfo *dinfo;
    BlockBackend *drive;
    MemoryRegion *mem;
    DeviceState *dev;
    Object *cpu;
    BusState *bus;
    qemu_irq irq[CXD90045_NUM_IRQ - CXD90045_IRQ_OFFSET];
    CXD90045LiroState *liro_state;
    int i;

    dinfo = drive_get(IF_MTD, 0, 0);
    drive = dinfo ? blk_by_legacy_dinfo(dinfo) : NULL;

    cpu = object_new(machine->cpu_type);
    object_property_set_bool(cpu, "has_el3", false, &error_fatal);
    object_property_set_bool(cpu, "reset-hivecs", true, &error_fatal);
    qdev_realize(DEVICE(cpu), NULL, &error_fatal);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "ddr0", CXD90045_DDR0_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD90045_DDR0_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "ddr1", CXD90045_DDR1_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD90045_DDR1_BASE, mem);

    liro_state = g_new0(CXD90045LiroState, 1);
    mem = g_new(MemoryRegion, 1);
    memory_region_init_io(mem, NULL, &cxd90045_liro_ops, liro_state,
                          "liro-regs", CXD90045_LIRO_SIZE);
    memory_region_add_subregion(get_system_memory(), CXD90045_LIRO_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "sram", CXD90045_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD90045_SRAM_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    /* The Linux kernel uses high vectors at 0xffff0000 and copies its
     * vector page there, so this must be writable RAM after reset. */
    memory_region_init_ram(mem, NULL, "vectors", CXD90045_BOOTROM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD90045_BOOTROM_BASE, mem);

    dev = qdev_new(TYPE_A9MPCORE_PRIV);
    qdev_prop_set_uint32(dev, "num-cpu", 1);
    qdev_prop_set_uint32(dev, "num-irq", CXD90045_NUM_IRQ);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90045_MPCORE_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ));
    for (i = 0; i < CXD90045_NUM_IRQ - CXD90045_IRQ_OFFSET; i++) {
        irq[i] = qdev_get_gpio_in(dev, i);
    }

    dev = qdev_new(TYPE_CXD90045_SUGI);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90045_SUGI_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       irq[CXD90045_IRQ_SUGI - CXD90045_IRQ_OFFSET]);

    for (i = 0; i < CXD90045_NUM_GPIO; i++) {
        dev = qdev_new("bionz_gpio");
        qdev_prop_set_uint8(dev, "version", 3);
        dev->id = g_strdup_printf("gpio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90045_GPIO_BASE(i));
    }

    dev = qdev_new("bionz_bootcon");
    /* Local port: serial0 is reserved for the PL011 UART used as Linux console. */
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90045_BOOTCON_BASE);

    dev = qdev_new(TYPE_SYSBUS_SDHCI);
    qdev_prop_set_uint8(dev, "uhs", UHS_II);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90045_SDHCI_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90045_IRQ_SDHCI - CXD90045_IRQ_OFFSET]);
    bus = qdev_get_child_bus(dev, "sd-bus");

    dev = qdev_new(TYPE_SD_CARD);
    qdev_prop_set_bit(dev, "emmc", true);
    qdev_prop_set_bit(dev, "high_capacity", true);
    qdev_prop_set_uint32(dev, "boot_size", 0x40000);
    qdev_prop_set_drive(dev, "drive", drive);
    qdev_realize_and_unref(dev, bus, &error_fatal);

    for (i = 0; i < CXD90045_NUM_UART; i++) {
        pl011_create(CXD90045_UART_BASE(i), irq[CXD90045_IRQ_UART(i) - CXD90045_IRQ_OFFSET], serial_hd(i));
    }

    for (i = 0; i < CXD90045_NUM_HWTIMER; i++) {
        dev = qdev_new("bionz_hwtimer");
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90045_HWTIMER_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90045_IRQ_HWTIMER(i) - CXD90045_IRQ_OFFSET]);
    }

    for (i = 0; i < CXD90045_NUM_SIO; i++) {
        dev = qdev_new("bionz_sio");
        dev->id = g_strdup_printf("sio%d", i);
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90045_SIO_BASE(i));
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD90045_SIO_BASE(i) + 0x100);
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90045_IRQ_SIO(i) - CXD90045_IRQ_OFFSET]);
    }

    if (machine->kernel_filename) {
        ssize_t initrd_size;
        /*
         * Sony's kernel expects the bootloader (EXBL) to have populated the
         * initial page tables, including the page holding swapper_pg_dir.
         * Without a bootloader, setup_arch faults reading the ATAGS.
         * The kernel is linked for PAGE_OFFSET 0x60000000 / PHYS_OFFSET 0;
         * before setup_arch reads the ATAGS it does __va(0x00010000), which
         * is VA 0x60010000.  Prepopulate that one 1 MiB section (the head
         * code maps the kernel image sections itself).
         */
        {
            uint32_t pgd[4096];
            memset(pgd, 0, sizeof(pgd));
            pgd[0x600] = (CXD90045_DDR0_BASE & 0xfff00000) | 0x00011c0e;
            uint32_t readback = 0;
            address_space_write(&address_space_memory,
                                CXD90045_DDR0_BASE + 0x00104000,
                                MEMTXATTRS_UNSPECIFIED, pgd, sizeof(pgd));
            address_space_read(&address_space_memory,
                               CXD90045_DDR0_BASE + 0x00105800,
                               MEMTXATTRS_UNSPECIFIED, &readback, 4);
            fprintf(stderr, "A7M3: wrote initial pgd at 0x%08x "
                    "(pgd[0x600]=0x%08x readback=0x%08x)\n",
                    (unsigned)(CXD90045_DDR0_BASE + 0x00104000),
                    (unsigned)pgd[0x600], (unsigned)readback);
        }
        load_image_targphys(machine->kernel_filename, CXD90045_DDR0_BASE + CXD90045_TEXT_OFFSET, CXD90045_DDR0_SIZE - CXD90045_TEXT_OFFSET);
        initrd_size = load_image_targphys(machine->initrd_filename, CXD90045_DDR0_BASE + CXD90045_INITRD_OFFSET, CXD90045_DDR0_SIZE - CXD90045_INITRD_OFFSET);
        cxd90045_write_atags(CXD90045_ATAGS_ADDR,
                             CXD90045_DDR0_BASE + CXD90045_INITRD_OFFSET,
                             initrd_size, CXD90045_CMDLINE);
        cxd_write_bootloader_with_machine(CXD90045_BOOTROM_BASE,
                                          CXD90045_DDR0_BASE + CXD90045_TEXT_OFFSET,
                                          0x11a, CXD90045_ATAGS_ADDR);
    } else if (bios_name) {
        load_image_targphys(bios_name, CXD90045_BOOTROM_BASE, CXD90045_BOOTROM_SIZE);
    } else if (drive) {
        cxd_write_bootloader(CXD90045_BOOTROM_BASE, cxd90045_init_loader2(drive));
    }

    cxd_add_const_reg("miscctrl_mode", CXD90045_MISCCTRL_BASE(1), 0x28);

    cxd_add_const_reg("emmc0", CXD90045_SDHCI_BASE + 0x124, 0x1000000);
    cxd_add_const_reg("emmc1", CXD90045_SDHCI_BASE + 0x130, 0x1fff);

    cxd_add_const_reg("ddrc0", CXD90045_DDRC_BASE(0) + 0x148, 0x20000000);
    cxd_add_const_reg("ddrc1", CXD90045_DDRC_BASE(1) + 0x148, 0x20000000);

    cxd_add_const_reg("unknown0", 0xf2908008, 1);
    cxd_add_const_reg("unknown1", 0xf290c008, 1);
}

static void cxd4108_machine_init(MachineClass *mc)
{
    mc->desc = "Sony BIONZ CXD4108";
    mc->init = cxd4108_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("arm926");
    mc->max_cpus = 2;
    mc->default_cpus = 2;
    mc->ignore_memory_transaction_failures = true;
}

DEFINE_MACHINE("cxd4108", cxd4108_machine_init)

static void cxd4115_machine_init(MachineClass *mc)
{
    mc->desc = "Sony BIONZ CXD4115";
    mc->init = cxd4115_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("arm11mpcore");
    mc->ignore_memory_transaction_failures = true;
}

DEFINE_MACHINE("cxd4115", cxd4115_machine_init)

static void cxd4132_machine_init(MachineClass *mc)
{
    mc->desc = "Sony BIONZ CXD4132";
    mc->init = cxd4132_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("arm11mpcore");
    mc->ignore_memory_transaction_failures = true;
}

DEFINE_MACHINE("cxd4132", cxd4132_machine_init)

static void cxd90014_machine_init(MachineClass *mc)
{
    mc->desc = "Sony BIONZ CXD90014";
    mc->init = cxd90014_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a5");
    mc->max_cpus = 2;
    mc->default_cpus = 2;// main + boss
    mc->ignore_memory_transaction_failures = true;
}

DEFINE_MACHINE("cxd90014", cxd90014_machine_init)

static void cxd90045_machine_init(MachineClass *mc)
{
    mc->desc = "Sony BIONZ CXD90045";
    mc->init = cxd90045_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a5");
    mc->ignore_memory_transaction_failures = true;
}

DEFINE_MACHINE("cxd90045", cxd90045_machine_init)
