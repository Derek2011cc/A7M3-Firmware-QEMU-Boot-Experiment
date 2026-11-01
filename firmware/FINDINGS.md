# A7M3 固件逆向记录（4.03，服务模式转储）

对象：**ILCE-7M3**，固件 **4.03**（`/setting/updater/dat4` = `03 04`）。
数据来源：相机 USB `054c:0336` 服务模式，`FileControl`（`0xff01`，
`cmd=2 READ`）只读读取 `/dev/nflasha*`。

> 本文件是公开版，只含分区表、文件名、哈希和分析结论，不含 Sony 二进制。
> 带实际文件的完整本机包放在仓库外的 `a7m3-firmware/`，可用
> `populate.sh` 从自己的转储重建。详见 [`README.md`](README.md)。

## 0. 结论摘要

- 共取得 **17 个分区/启动分区转储**，合计 **1,358,888,960 B**（约 1.36 GB）。
- 主 Linux 启动对在 **`nflasha3`**：`VMLINUX.BIN`（4,117,760 B）+ `INITRD.IMG`（2,178,048 B）。
- **`nflasha1/BOOT`** 还有第二套启动对：`VMLINUX.BIN`（1,697,104 B，**zImage**）+
  `INITRD.IMG`（4,354,048 B，**cramfs**，562 个文件）。已确认是
  **updater/recovery 环境**（见 §5）。
- **两份不同的引导参数**：
  - `nflasha2:/kemco.txt`（1,901 B，SHA-256 `83b32a57...`）是**主内核**参数，
    含 `mem=1G@0`、`mem=256M@0x80000000@1`、`memrsv=704M@0x14000000:uc`、
    `memrsv=14M@0x80000000:uc`、`memrsv=242M@0x80e00000:uc` 等；
  - `nflasha1/BOOT/KEMCO.TXT`（815 B）是 **updater/recovery** 参数，
    含 `mem=1016M@0x80000000@1`、`memrsv=0x3600000@0x20000000:uc`。
  详见 §3.0 与 §3.1。
- 主内核为 Linux **3.0.27_nl-rt106+**（SMP PREEMPT RT，2025-06-19 构建），
  ARM32；QEMU 中已启动到相机 BusyBox root shell。
- Appframework 的崩溃根因是 **DFB 在 1 扇区的 `nflasha4` 上写失败后二次加锁
  MemInfo 互斥量**；把 DFB 目标改到 `/dev/nflasha16` 后，`im.elf` 稳定运行到
  **63 线程**。GPU 时钟/`dmpgles2` 节点是更早的阻塞，已通过实验绕过。
- 当前主阻塞是 **LIRO/CP 固件对未建模 `0xf2a1xxxx` 硬件的忙等**：
  `entry_main` 返回后固件循环读 `0xf2a1102c` bit1，RT 线程抢死 Linux；
  UI 库未加载，`bootin.elf` 停在 `osal_rcv_msg_tmo`。
- 尚未解开：`nflasha4`（**CMMeX**）与 `nflasha5`（**WBI1**）容器、
  `nflasha1` recovery 启动对、LIRO/CP 与 GPU/显示/传感器模型。

## 1. 获取方式与版本

| 项目 | 值 |
|---|---|
| 设备 | ILCE-7M3（A7 III），CXD90045，ARM32 Cortex-A5 |
| 固件 | 4.03（`dat4` = `03 04`） |
| USB | 服务模式 `054c:0336` |
| 协议 | Senser `FileControl`，`pFunc 0xff01`，`cmd=2 READ` |
| 访问 | 只读；未写入相机分区 |
| 工具 | vendored Sony-PMCA-RE + pyusb |

注意：手头的官方更新包是 **4.04**，与本机 4.03 转储不是同一版本。
交叉核对结果：

- 相机 `/sbin/init` 与 4.04 包 initrd 的 `/sbin/init` **一致**（本记录中
  `nflasha7/sbin/init` 与 `initrd/sbin/init` 的 SHA-256 都是
  `2258c8d964e6e3019c5c06fa90b0d01a9c60ad50ea2bbf4a1c35951d7a43d63a`）；
- `/usr/bin/launch_shell.elf` 与 4.04 包同名文件一致；
- `/usr/lib/libJiritsu.so` 在 4.03/4.04 之间**不同**。

所以本记录中的偏移、哈希只保证对应 **4.03**；4.04 不能直接套用。

## 2. 分区地图

大小与 SHA-256 来自只读转储；完整表格见 [`MANIFEST.md`](MANIFEST.md)。
`/setting`、`/log` 等分区只列大小/类型，不公开其中可能属于设备私有的
文件内容。

| 分区 | 大小 (B) | 文件系统/容器 | 启动时挂载/角色 | 关键内容 |
|---|---:|---|---|---|
| nflasha1 | 8,323,072 | FAT12 | updater/recovery boot 分区（EXBL 可读） | `/BOOT`：第二套 zImage + cramfs、`KEMCO.TXT`、`LOADADDR.TXT`；`P1CHKSUM`、`DAT2`、`DAT3` |
| nflasha2 | 20,971,520 | FAT12 | `/setting` | `BACKUP.BIN`/`BACKUP.BAK`（各 1,153,364 B）、`PSD.BIN` 4,427,776 B、`FACEREC0..7`、`DMMCON~1.BIN`、`ULOGIO.BIN`、`UPDATER/DAT4`、`APPFW_~1/`、`MODE/`、`GPS/` |
| nflasha3 | 50,331,648（FS 用 31,493,120） | FAT12 | `/system` | **主启动对** `VMLINUX.BIN` 4,117,760 + `INITRD.IMG` 2,178,048；`AV-CAM.BIN` 18,449,784；`BONOBO.BIN`；`DFE_APP/DAT`；`LDR_DRV.BIN`；`LENSBI~1/`；`SABIN/`（`IDT_CAM`、`SA_DFDET`、`SSBOOT`）；`LIF_APP.BIN` |
| nflasha4 | 12,582,912 | Sony **CMMeX** 容器 | 未挂载/未解 | magic `CMMeX`；头部之后有记录式数据（未解码） |
| nflasha5 | 62,914,560 | Sony **WBI1** 容器 | 用途未确认 | magic `WBI1`；未展开 |
| nflasha6 | 16,777,216 | FAT12 | — | 根目录为空 |
| nflasha7 | 8,388,608 | ext2（1 KiB 块） | `/rootfs` → 切换后 `/` | **相机 rootfs**：`/bin/busybox` 433,504、`/sbin/init` 28,572、`/kmod` 早期模块、glibc 2.11.2、`/version.txt` = `37114_12.2025061901` |
| nflasha8 | — | — | — | 不存在/不可读（Senser 130） |
| nflasha9 | — | — | — | 不存在/不可读（Senser 130） |
| nflasha10 | 12,582,912 | FAT12 | `/tmp` 与 `/etc` | `IMAGES/`（字体 `.TTF`/`.LTT`）、`JIRITSU/`、`KIKILOG.DAT` 622,592、`DHCPD` 配置、`MTAB` 等 |
| nflasha11 | 524,288,000 | FAT16 | `/log` | `ACCESS~1.DAT`/`.BAK`（各 2,097,220 B）；其余基本为空 |
| nflasha12 | 41,943,040 | FAT12 | `/cert` | 根目录为空 |
| nflasha13 | 4,194,304 | FAT12 | — | `/HASH/`、`/HISTORY/UPDATE.LOG`（1,151 B） |
| nflasha14 | — | — | — | 不存在/不可读（Senser 130） |
| nflasha15 | 314,572,800 | ext2（1 KiB 块） | `/usr` | `/bin`：`im.elf` 23,240、`launch_shell.elf` 5,140、`sen.elf`、`sndcmd.elf`、`gpioctrl.elf` 等；`/kmod`：`grm_gles.ko`、`grm_ma.ko`、`upm.ko`、`dmm.ko`、`backup.ko`、`udate.ko`、网络/USB 模块；`/lib`：Sony 基础库、OpenSSL、glibc 等；`/sabin`：传感器/图像算法 `.bin` |
| nflasha16 | 146,800,640 | 全零 | — | 整个文件 **146,800,640 字节全为 0**（已验证） |
| nflasha17 | 104,857,600 | FAT16 | — | 根目录为空 |
| nflasha18 | 20,971,520 | FAT12 | `/lens` | `VX8900...`、`VX8901...` 等镜头数据文件（每个 1.5–8 KB） |
| nflashaB0 | 4,194,304 | Sony **EXBL** 引导镜像 | boot0 | magic `EXBL`；非零字节 72,742；字符串含 `(C)2016 Sony Corp. DIG/PD/SD-2`、`V85.00.03`、`DGU5` |
| nflashaB1 | 4,194,304 | 全零 | boot1 | 4 MiB 全 0 |

不存在的设备节点：`nflasha0`、`nflasha8`、`nflasha9`、`nflasha14`、`nflashaB2`。

分区内容的逐文件清单见 [`listings/`](listings/)（公开的固件分区）以及本机
完整包中的 `listings/`（含 `/setting`、`/log` 等私有分区，不发布）。

## 3. 重点发现：真实引导参数

### 3.0 主内核参数：`nflasha2:/kemco.txt`（2026-10-31 晚补）

主内核真正使用的参数**不是** `nflasha1/BOOT/KEMCO.TXT`，而是
`nflasha2`（FAT12，挂载为 `/setting`）根目录下的 `kemco.txt`：

| 项目 | 值 |
|---|---|
| 路径 | `nflasha2:/kemco.txt` |
| 大小 | 1,901 字节 |
| SHA-256 | `83b32a57278ee640e27fb60cbeec28a9c74b6055f5fe1b2b728d1c053b5988c8` |
| 合成 eMMC 镜像中的文本位置 | 文件偏移 `0x9bcad0`（`nflasha2` 分区内偏移 `0x3cad0`） |

与内存/协处理器相关的原文摘录（完整 1,901 字节文件不随仓库分发，
只保留与建模相关的行）：

```
console=ttynull xrstreq=1 wdt.mode=1 stack_usage=n cxd900x0.bam=N mmu.dac=Y exec.coremode=1
rootfstype=ext2 root=/dev/ram0 init=/sbin/init initrd=0x700000,8M
pm.resume=0x20021054 pm.sus_error=0x20021060
pm.ddr_ctrl=sref pm.ddr_ctrl2=sref
wbi_memclr=0x200210e8
em_console=0x200210ec
wbimem=0x1000@0x20021000
wbimem=0xEC000@0x20028000
wbimem=0x8000@0x20145000
wbimem=0x500000@0x03085000
wbimem=0x96D000@0x10000000
wbimem=0x2916000@0x20200000
wbimem=0x200000@0x23E00000
cmpr.mem=0x8000@0x14000000
warm.mem_dst=0xC00000@0x14100000
warm.mem_work=0x600000@0x14D00000
boottime=0x20000@0x03585000
klog.addr=0x035A5000 klog.size=0x20000
alog.addr=0x03524FC0 alog.size=0x60040 alog.clock=cpu
mem=1G@0
memrsv=1M@0
memrsv=0x8000@0xF00000
memrsv=0x1A5E000@0x02E68000
memrsv=0xA6EB000@0x09915000
memrsv=704M@0x14000000:uc
mem=256M@0x80000000@1
memrsv=14M@0x80000000:uc
memrsv=242M@0x80e00000:uc
*
```

内存地图（与 updater 参数的关键差异）：

- DDR1 是 **256 MiB @ `0x80000000`**，不是 1016 MiB；
- DDR1 由 `14M` + `242M` 两段 `uc` 完整覆盖；
- 低端 `0x14000000..0x3FFFFFFF` 是 704 MiB `uc`；
- LIRO 的 `0x8bdc1200` 落在 DDR1 的 242 MiB `uc` 区间内。

> 证据边界：尚未在串口日志中直接抓到 bootloader/init 读取该文件的调用。
> 将其作为主内核参数使用的依据是：文件位于 `/setting`、以 `kemco` 开头、
> 内容是完整启动参数，且按其中内存行启动后 LIRO 的非缓存转换行为与预期
> 一致。详见 [`../docs/LIRO-AND-MEMORY.md`](../docs/LIRO-AND-MEMORY.md) §1。

### 3.1 `nflasha1/BOOT/KEMCO.TXT`（updater/recovery）

文件 815 字节，内容如下（原样）：

```
kemco
ip=off initrd=0x00700000,0x00800000 root=/dev/ram0
rw noatime noalign
init=/sbin/init
heap_profile.records=0
snsc_workqueue.kipid=130 manage.irqd=129 softirq.sirqd=128
exception.reboot=1 exception.console_option=115200n8
softlockup_panic=0 softlockup_thresh=30
wdt.timeout=500
warm.boot_drop=0
warm.ignore_drop=1
pcie.refclk=9,2 pcie.pwr1=2,18 pcie.pwr2=2,23 pcie.pwr3=2,22
error.led=0,0
cfg.poff=0,10
console=ttyAM0,115200n8 amba2.console=0 xrstreq=0 wdt.mode=0 stack_usage=y cxd90014.bam=Y
cfg.uart=15
tmonitor.addr=0x1A00000 tmonitor.size=0x500000 tmonitor.mask=0
wbi_memclr=0x200210e8
em_console=0x200210ec
boottime=0x20000@0x03480000
klog.addr=0x034A0000 klog.size=0x20000
mem=1G@0
memrsv=1M@0
memrsv=0x75e000@0x02E68000
memrsv=0x500000@0x09b00000
memrsv=0x3600000@0x20000000:uc
mem=1016M@0x80000000@1
*
```

### 3.2 `nflasha1/BOOT/LOADADDR.TXT`

```
0x00108000,0x00700000,0x00103000
```

三个值依次与主内核加载地址 `0x00108000`、initrd 地址 `0x00700000` 一致；
第三个 `0x00103000` 很可能是参数/ATAGS 区（本实验的 QEMU 把 ATAGS 放在
`0x00010000`，读法相同，地址不同）。

### 3.3 这组参数说明了什么

> 归属：这套参数位于 `nflasha1/BOOT` 的 **updater/recovery 启动对**；
> 该 cramfs 的 `/sbin/init` 是 updater 脚本（见 §5）。因此 `KEMCO.TXT`
> 是 updater 内核的 cmdline。**主内核不复用这份参数**：主内核使用
> `nflasha2:/kemco.txt`（见 §3.0），两者的 DDR1 大小和 `uc` 区间不同。

1. **updater 内核的内存地图与 QEMU 当前模型不同**：
   - `mem=1G@0`：低 1 GiB；
   - `memrsv=1M@0`：保留最低 1 MiB；
   - `memrsv=0x75e000@0x02E68000`：约 7.37 MiB @ 48.6 MB；
   - `memrsv=0x500000@0x09b00000`：5 MiB @ 155 MB；
   - `memrsv=0x3600000@0x20000000:uc`：**54 MiB 非缓存窗口** @ 512 MB；
   - `mem=1016M@0x80000000@1`：第二段 1016 MiB @ 2 GiB（尾部 `@1` 语义未确认）。
2. **`0x20000000` 窗口确实是 MMIO**：`em_console=0x200210ec`、
   `wbi_memclr=0x200210e8`，以及 `upm.ko` 访问的 `0x2002104c` 都落在
   `0x20000000..0x23600000` 内。这解释了为什么 `arch_phys_to_noncache`
   必须把这窗口映射出来。
3. **QEMU 的当前 cmdline 已改用主内核参数**：
   早期实验用 `mem=1G@0` + `memrsv=0x20000000@0x20000000:device`（把整整
   512 MiB 当设备窗口）只是可启动的简化近似。现在 QEMU 使用 §3.0 的
   主内核地图：DDR1 256 MiB + 704M/14M/242M `uc`。updater 的 54 MiB
   `uc` 窗口 + 1016 MiB 第二段**不是**主内核配置。
4. **控制台门控参数**：真实 cmdline 含 `amba2.console=0`、
   `exception.console_option=115200n8`，与 Sony 的 errind/ADC 控制台门控
   相关。本实验用内核补丁强制打开 PL011，没有模拟这段逻辑。
5. **异常监视器**：`exception.reboot=1` 解释了 Appframework 失败后进入
   `Exception>` 监视器的行为。

> **修正（2026-10-31 晚）**：主内核参数已定位到 `nflasha2:/kemco.txt`
> （§3.0），不是 `nflasha1/BOOT/KEMCO.TXT`。后者只用于 updater/recovery。
> 主内核的 `initrd=0x700000,8M`、`root=/dev/ram0`、`init=/sbin/init`
> 与 updater 一致，但 `mem=`/`memrsv=` 不同。

## 4. 主内核与 initrd

### 4.1 主内核

| 项目 | 值 |
|---|---|
| 文件 | `nflasha3:/VMLINUX.BIN` |
| 大小 | 4,117,760 B |
| SHA-256 | `13649dc02443d761df58ba2745171acb089f7865c12fd384df1191e2c45c5cd6` |
| 格式 | 原始 ARM Image（非 zImage） |
| 版本 | Linux `3.0.27_nl-rt106+` #1 SMP PREEMPT RT |
| 构建 | 2025-06-19 18:15:47 JST，gcc 4.5.1（Sony CE Linux 8.2.7.6） |
| 链接基址 | `0x60108000`；`PAGE_OFFSET=0x60000000`；`TEXT_OFFSET=0x108000` |
| 物理加载 | `0x00108000` |

内核 head 期望 bootloader（EXBL）已经建立初始页表；在 QEMU 中需要预置
`pgd[0x600]=0x00011c0e` 于 PA `0x00104000`，并打补丁跳过内核自己的页表清零
（VA `0x60108068`，`0xE4803004` → `0xEA000004`）。

Sony stext 在 `0x6011efa0` 会把 `r2`（ATAGS 指针）清掉；需要在
`0x60108028` 改调桩（`0x60495f40`），桩加载 `0x00010000` 后跳回
`__vet_atags`（`0x60108118`）。

控制台门控在 `0x602988cc`：`str r3,[r6,#0xd4]` → `str r2,[r6,#0xd4]`
（`0xE58630D4` → `0xE58620D4`），强制打开 PL011 输出。

以上补丁的完整脚本在 [`../scripts/`](../scripts/)。

### 4.2 主 initrd

| 项目 | 值 |
|---|---|
| 文件 | `nflasha3:/INITRD.IMG` |
| 大小 | 2,178,048 B |
| SHA-256 | `0791900525b4e5040edd0b233fb513bc242a3480eca88feac5c355410def9d10` |
| 格式 | **ext2 文件系统镜像**（不是 cpio） |
| 挂载 | 内核 `root=/dev/ram0`，日志 `rootfs image is not initramfs ... looks like an initrd` |

根目录：`/sbin/init`（28,572 B）、`/kmod`、`/lib`、`/rootfs`（空挂载点）、
`/dev`、`/proc`。`/sbin/init` 与 `nflasha7:/sbin/init` 完全一致。

`/sbin/init` 内嵌模块加载列表，包括：

```
/kmod/utimer.ko
/usr/kmod/backup.ko
Backup_load=0
/usr/kmod/osal_ulogio.ko
ulogio_initfile=/setting/ulogio.bin
/usr/kmod/input.ko
/usr/kmod/udate.ko
/usr/kmod/indctr.ko
/usr/kmod/i2c.ko
/usr/kmod/sircs.ko
...
```

QEMU 下需要两处等长修改（详见 [`../DESIGN.md`](../DESIGN.md) §7）：

- `Backup_load=0 ` → `Backup_load=1 `（镜像偏移 2,016,076）
- `/usr/kmod/udate.ko` → `/usr/kmod/udate.zz`（镜像偏移 2,016,172）

### 4.3 启动链（QEMU 已验证）

1. QEMU 加载内核到 `0x00108000`，initrd 到 `0x00700000`，ATAGS 到 `0x00010000`；
2. 内核以 `r1=0x11a` 启动，读 ATAGS；
3. initrd 以 ext2 挂到 `/dev/ram0`；
4. `/sbin/init` 挂载 `nflasha2/3/7/10/11/12/15/18`；
5. 先尝试 `dnflasha7/15` 失败（没有 `ptbl`/WBI），回退到 `nflasha7/15`；
6. 加载早期模块与 `/usr/kmod` 模块；
7. 启动 `/usr/bin/im.elf`；
8. 未打 DFB 补丁时 Appframework 失败，进入 `Exception>`；`exit` 回到
   BusyBox `/ #`。打了 `scripts/experimental/patch-libmpr-dfb16.py`
   的副本后，`im.elf` 可运行到 63 线程，可能不再出现该提示。

## 5. 第二套启动对（nflasha1/BOOT）

| 文件 | 大小 | 格式 | SHA-256 |
|---|---:|---|---|
| `VMLINUX.BIN` | 1,697,104 | ARM **zImage** | `b5c68ff96cdac4b5e86027d63698dbd3aa9340a86cb06e13d96f549cdffd92e8` |
| `INITRD.IMG` | 4,354,048 | **cramfs**（v2、562 个文件） | `e92f3344ce92c83d3c9592121cbcf6c43fdf28b0c659f84ee3c2a13cc9157804` |
| `KEMCO.TXT` | 815 | 文本引导参数 | `0f9c5cd243da4ebbb412bc1d34d3752a6e7d875ed5db6f4034d53029c3f5a4e7` |
| `LOADADDR.TXT` | 33 | 文本加载地址 | `a55bb7c1ef670cad21301679f75412bf2f81bfe713c3a83acd4b2b457523200d` |
| `P1CHKSUM` | 105 | 校验数据 | 未分析 |
| `DAT2` / `DAT3` | 4 / 4 | 小数据 | 未分析 |

这套 zImage+cramfs 与主启动对（raw Image+ext2）明显不同。展开 cramfs 后
可以确认它是 **updater/recovery 环境**（完整 562 个文件的名称/大小见
[`listings/recovery-cramfs.txt`](listings/recovery-cramfs.txt)）：

- `version.txt` = `100.101.005`；`ramdsk` = `tmp_updater`；共 562 个文件；
- `/sbin/init` 是 shell 脚本：挂载 proc/sys、把 rootfs 重新挂为 rw、设置
  2011 年时间、`mkramdsk.sh`（32 MiB tmpfs 到 `/ramdsk`）、`insmod.sh`、
  power IC 初始化、启动 `UdtrMain.sh &`，最后进入 `/bin/ash --login`
  （存在 `/root/IAMLDUDWRT` 时需要输入硬编码的 updater 密码；具体值见
  本地包 `files/extracted/recovery/sbin_init`，不在公开文档中复制）；
- `usr/bin/insmod.sh`：加载 `devno`、`dmac`、`ldec`、`emmc tbl=0x80000000`、
  `osal_utm`、`osal_uipc`、`utimer`、`stream`、`stream2`、`sio`；若存在
  `IAMLDUDWRT`，从串口配置取出 `Backup.bin`/`ulogio.bin`/`DmmConfig.bin`
  并以 `Backup_load=1` 加载 `backup.ko`；否则挂载 `/dev/nflasha2`（vfat）
  并以 `Backup_save=0` 加载；随后加载 `osal_ulogio`、`dmm`
  （`DmmConfig=/etc/DmmConfig.bin`）、`i2c`、`ca`；
- `usr/bin/UdtrMain.sh`：通过 `/setting/updater/mode1/2/3/5/6` 判断
  production/minor/service/version-skip/MS-crypt 模式，设置 UFP 标记，
  启动 `ud_im.elf`，再执行 `execute_prod.sh` 或 `execute_ufp.sh`；
- `usr/bin/is_valid_boot.sh`：判断 OneNAND / MS / USB 启动源。

因此 `nflasha1/BOOT` 是**升级/恢复启动**，不是主相机启动；这也解释了
`Backup_load=1` 的语义。它与 `nflashaB0` EXBL（`V85.00.03`）如何配合，
仍待进一步确认。

## 6. rootfs 与用户态

### 6.1 `nflasha7`（rootfs）

- `/bin/busybox`：433,504 B，SHA-256 `a10bf06b887e18bd3388836fa226e16cbe96138849131b2a4a075a7216e553c7`；
- `/sbin/init`：28,572 B，与 initrd 的同名文件相同；
- `/lib/libc-2.11.2.so`：984,292 B，SHA-256 `bc93700df79ac8f67356e4dbf5ab5c4c656865455e18ef2860acc178f5186bc2`；
- `/version.txt`：`37114_12.2025061901`；
- `/kmod`：`nand.ko`、`lld.ko`、`dmac.ko`、`emmc.ko`、`ldec.ko`、`devno.ko`；
- `/usr` 只有 `lib/`，真正的 `/usr` 由 `nflasha15` 覆盖。

### 6.2 `nflasha15`（/usr）

`/bin` 是相机应用与工具目录，包含：

- `im.elf`（23,240 B，Appframework，SHA-256 `de26c4b208d9d4524a0d0e76474b626ddd5902a868c2b271d19ec1c7caffdbe5`）；
- `launch_shell.elf`（5,140 B，SHA-256 `5561b8f2fe92620e48ba1e4bca8049b24d84392345ac3f888145f6859fe3c526`）；
- `sen.elf`、`sndcmd.elf`、`gpioctrl.elf`、`bootin.elf`、`scenario.elf`、`ulogio.elf`、`crypter.elf`、`Wltestcmd`、`bsa_server`、`bt_firm.hcd` 等。

`/kmod` 是主要驱动目录，含：

| 模块 | 大小 (B) | 已探明情况 |
|---|---:|---|
| `grm_gles.ko` | 15,648 | Sugilite GLES 驱动；`sugi_hw.c:221` BUG；注册 `dmpgles2`（248:29） |
| `grm_ma.ko` | 24,800 | 图形/媒体相关（未深入） |
| `upm.ko` | 134,904 | 需要 `arch_phys_to_noncache(0x2002104c)`；memrsv 正确后加载 |
| `dmm.ko` | 83,320 | 加载成功 |
| `backup.ko` | 73,764 | `Backup_load=0` 时 `-EIO`；改 1 后文件加载成功 |
| `udate.ko` | 14,836 | `udate_init_clock` NULL 解引用；需跳过 |
| `sd_mod.ko`、`liro.ko`、`stream2.ko` | — | 已随系统加载/未逐一分析 |
| `bcmdhd.ko` | 522,236 | Wi-Fi（QEMU 未模拟） |
| `r8168.ko` | 423,556 | 有线网卡（QEMU 未模拟） |
| 其他 | — | USB、音频、NFC、SCSI、PPP 等 |

`/sabin` 存放传感器/图像算法数据，例如 `sa_aed.bin`、`sa_integdec.bin`、
`sa_jpeg_enc_1core.bin`、`spica_musashi_*` 等；这些是后续传感器/ISP
模拟的重要输入，但尚未解析格式。

### 6.3 Appframework 卡点与 2026-10-31 进展

早期（`init_module` 整体打桩）的路径：

1. `FixedArea` `mmap` 失败或返回 `FFFFFFFF`；
2. 打开 `/dev/dmpgles2` 失败（模块被整段打桩，不注册设备；当时误记为
   `/dev/sugilite`）；
3. abort 路径，libc `tgkill`，PC `0x04313796`（libc offset `0x15796`）；
4. init 落入 `Exception>`。

2026-10-31 的新实验把驱动真正注册起来：

1. 只改 `sugilite_clock_up_internal` 结尾的 `cmp r0,#0x15; ldmeq` 为
   `mov r0,#0; ldm`，并恢复原始 `init_module`；
2. `grm_gles` init 返回 0，注册字符设备 **`dmpgles2`（major 248,
   minor 29）**；dev_t 来自 `devno.ko` 的 `udif_devnodes` 表；
3. 模块不创建 `/dev` 节点，rootfs `/dev` 是静态 ext2；离线加入
   `dmpgles2 c 248 29`；
4. Appframework 打开并 mmap 该设备 128 KiB，`FixedAreaTop=20098000`、
   `FixedAreaSize=0007C000`（不再全 F）；
5. 新失败点：`THREAD_INFRA1` 在 `libmpr.so` 调用
   `__pthread_mutex_lock`，`r0=0x1c`（NULL `this+0x1c`），页故障
   `0x28`；一次符号化为 `MemInfo::StreamAreaFree`。ASLR 下绝对地址变化，
   给 `libmpr.so` 打单点 `bx lr` 补丁没有效果。

结论：设备打开/mmap 已不是阻塞。后续反汇编确认，`libmpr` 崩溃的直接原因是
**DFB 在 1 扇区的 `nflasha4` 上写失败后的二次加锁**；把 DFB 目标改到
`/dev/nflasha16` 后，`im.elf` 可稳定运行到 **63 线程**。当前主阻塞转移到
LIRO/CP 的 `0xf2a1xxxx` 硬件忙等和 UIPC 握手，详见
[`../docs/LIRO-AND-MEMORY.md`](../docs/LIRO-AND-MEMORY.md)。

## 7. 已确认 / 部分确认 / 未确认

| 项目 | 状态 | 证据 |
|---|---|---|
| 17 个分区/启动分区的大小/类型 | 已确认 | 只读转储 + `file`/`fsck` 识别 |
| 主内核版本/加载地址 | 已确认 | 内核日志、`LOADADDR.TXT` |
| initrd 为 ext2、`/sbin/init` | 已确认 | `file`、debugfs、内核日志 |
| 主内核引导参数（`nflasha2:/kemco.txt`） | 已找到原文 | 1,901 B；SHA-256；内存部分已在 QEMU 复现 |
| updater 引导参数（`nflasha1/BOOT/KEMCO.TXT`） | 已找到原文 | 815 B；仅 updater/recovery，主内核不复用 |
| QEMU 启动到 BusyBox shell | 已确认 | 串口日志 `/ #` |
| 分区挂载与 `dnflasha` 回退 | 已确认 | 内核日志 |
| backup/udate/grm_gles 失败点 | 已确认 | 串口日志、模块反汇编 |
| grm_gles 设备节点 | 已确认 | `dmpgles2` 248:29；open/mmap 128 KiB 成功 |
| Appframework DFB 崩溃 | 已确认 | `nflasha4` 1 扇区；DFA::FlashWrite 错误路径二次加锁 |
| DFB 绕过 / im.elf 63 线程 | 已确认 | `/dev/nflasha16`；`Threads: 63` |
| LIRO 固件执行 / 忙等 | 部分确认 | `entry_pre`/`entry_main` 返回；134+ `liro-kliro_*` 线程；`0xf2a1102c` bit1 忙等 |
| nflasha1 updater/recovery 启动对 | 已确认 | zImage/cramfs/LOADADDR、cramfs 562 文件与 init 脚本 |
| CMMeX / WBI1 容器 | 未解 | 仅识别 magic |
| nflasha16 全零 | 已确认 | 整文件扫描 |
| 真机 `/proc/iomem` | 未核对 | 需要只读服务/运行态 |
| GPU/显示/传感器/拍摄链 | 未实现 | — |
| 4.03/4.04 差异 | 部分确认 | init/launch_shell 一致，libJiritsu 不同 |

## 8. 文件哈希（关键文件）

| 文件 | 大小 (B) | SHA-256 |
|---|---:|---|
| `nflasha3/VMLINUX.BIN` | 4,117,760 | `13649dc0...c5cd6` |
| `nflasha3/INITRD.IMG` | 2,178,048 | `07919005...9d10` |
| `nflasha1/BOOT/VMLINUX.BIN` | 1,697,104 | `b5c68ff9...d92e8` |
| `nflasha1/BOOT/INITRD.IMG` | 4,354,048 | `e92f3344...57804` |
| `nflasha7/sbin/init` = `initrd/sbin/init` | 28,572 | `2258c8d9...3d63a` |
| `nflasha7/bin/busybox` | 433,504 | `a10bf06b...553c7` |
| `nflasha7/lib/libc-2.11.2.so` | 984,292 | `bc93700d...86bc2` |
| `nflasha7/version.txt` | 19 | `445839d0...7a904` |
| `nflasha15/bin/im.elf` | 23,240 | `de26c4b2...fdbe5` |
| `nflasha15/bin/launch_shell.elf` | 5,140 | `5561b8f2...c526` |
| `nflasha15/lib/libmpr.so` | 12,516,284 | `ab1df757108d123f403188e33b732bf0c8cded33ff29711417dca14c5ab883a5` |
| `nflasha3/AV-CAM.BIN` | 18,449,784 | `e1b0f84e8323005a34b43de6dd0083fca712d2c654f0d9716512e0a54631bbe7` |
| `nflasha15/kmod/grm_gles.ko` | 15,648 | `32ba2728...d81af` |
| `nflasha15/kmod/upm.ko` | 134,904 | `9a10fe58...76df3` |
| `nflasha15/kmod/backup.ko` | 73,764 | `2d26bbdf...0167e` |
| `nflasha15/kmod/udate.ko` | 14,836 | `87c77795...50238` |
| `nflasha2/BACKUP.BIN` | 1,153,364 | `a8d95984...b3c27` |
| `nflasha2/kemco.txt` | 1,901 | `83b32a57278ee640e27fb60cbeec28a9c74b6055f5fe1b2b728d1c053b5988c8` |
| `nflasha2/UPDATER/DAT4` | 2 | `0ce3940b...f82947` |
| `nflasha1/BOOT/KEMCO.TXT` | 815 | `0f9c5cd2...5a4e7` |

完整分区哈希见 [`MANIFEST.md`](MANIFEST.md)；完整文件摘要见本机包
`MANIFEST.sha256`。

## 9. 包内结构

```
firmware/
├── FINDINGS.md          本文件
├── MANIFEST.md          分区清单（大小/哈希/识别）
├── PARTITIONS.tsv       机器可读分区表
├── README.md            使用说明与发布边界
├── populate.sh          从自己的转储重建 files/
├── tools/fat-extract.py FAT12/16 列表/提取
├── listings/            各分区文件清单（公开的固件分区）
└── files/               本机完整包才有：Sony 二进制（git 忽略）
    ├── extracted/kemco-main.txt   nflasha2 主内核参数
    ├── extracted/av-cam.bin       LIRO 固件
    ├── extracted/libmpr.so        运行时 libmpr（DFB 分析）
    └── ...
```

`files/` 在公开仓库中不存在；`populate.sh` 会从
`$HOME/firmware/a7m3/service-dump` 生成本地文件。不要把 Sony 二进制提交到
GitHub。

## 10. 下一步（按价值排序）

1. **已完成**：按主内核 `nflasha2:/kemco.txt` 重建内存模型（DDR1 256 MiB、
   704M/14M/242M `uc`）；`arch_phys_to_noncache(0x8bdc1200)` 报错消失。
   剩余：`wbi_memclr`/`wbimem`/`pm.*` 等参数尚未逐项模拟。
2. **LIRO/CP**：给 `0xf2a1xxxx` 做最小寄存器模型（先让 `0xf2a1102c`
   bit1 可读），用 GDB 逐个定位后续忙等点；这是原生 UI 之前的主阻塞。
3. **UIPC**：检查 `bootin.elf` 的 `osal_rcv_msg_tmo` 等待与 LIRO 侧端点，
   确认 UI 库未加载是否由缺少 CP 消息导致。
4. **显示**：Sugilite/panel/ramfb 模型另立任务；A7M3 是 ARM32 + 不同图形栈，
   不能直接复用 A7C2 的 runtime。
5. 展开 `nflasha4` CMMeX 与 `nflasha5` WBI1 容器；当前 DFB 用
   `/dev/nflasha16` 绕过，不是真实模拟。
6. 解析 `nflasha15/sabin` 的传感器/ISP 数据格式。
7. 已确认 `nflasha1/BOOT` 是 updater/recovery；解析 zImage/cramfs 的
   升级流程与 `ud_im.elf`。
8. 只读核对真机 `/proc/iomem`、`/proc/cmdline`（若服务模式能提供）。
