# 设计说明：在 QEMU 中启动 A7M3 固件

本文记录让 ILCE-7M3（CXD90045，ARM32 Cortex-A5）原厂 Linux 内核在
ma1co/qemu 的 `cxd90045` 机器上启动所做的全部模型侧与固件副本侧改动，
以及每一步的动机和判据。实现细节以 `scripts/` 与
[`patches/qemu-a7m3.patch`](patches/qemu-a7m3.patch) 为准；本文解释“为什么”。

> 所有地址均为本实验使用的 4.03 固件版本上的观察值。不同固件版本的可执行文件
> 布局可能不同，脚本在写入前会校验原始字节/指令，不匹配即拒绝。

## 1. 目标与边界

- 只用 QEMU 的 `cxd90045` 机器 + 原厂内核/initrd/分区镜像，不修改真机。
- 尽量少改固件：内核只改 7 个 32 位字，initrd 只改两个等长字符串，
  rootfs 只把 `grm_gles.ko` 的 `init_module` 换成返回 0 的桩。
- 目标优先级：先到 BusyBox shell（可研究用户态），再尝试 Appframework，
  最后才是 GPU/显示。

## 2. 启动协议

### 2.1 加载地址

Sony 内核链接在 `0x60108000`，但 `PHYS_OFFSET=0`、`PAGE_OFFSET=0x60000000`，
所以链接地址是虚拟地址，物理加载地址是：

| 项目 | 物理地址 | 说明 |
|---|---|---|
| 内核 | `0x00108000` | `TEXT_OFFSET=0x108000` |
| initrd | `0x00700000` | `CXD90045_INITRD_OFFSET` |
| ATAGS | `0x00010000` | QEMU 写入；内核用 `__va()` 读取 |
| 初始页表 | `0x00104000` | QEMU 预置（见 §3） |

内核第一条指令通过 `__pa` 关系直接执行，不需要 MMU 先建立完整映射；
但 `setup_arch` 读 ATAGS 时已经使用 `PAGE_OFFSET` 的虚拟地址，因此
`0x60000000` 段必须在进入 C 代码前就可翻译。

真机 `nflasha1/BOOT/LOADADDR.TXT` 给出
`0x00108000,0x00700000,0x00103000`；第三个值很可能是参数区。本实验把
ATAGS 放在 `0x00010000`，内核只通过 `r2` 读取，地址本身不影响启动。

### 2.2 ATAGS

QEMU 侧按 ARM32 Linux 引导协议写入（注意 Sony 内核接受的是 **size 在前** 的
`struct tag_header`）：

```
ATAG_CORE     0x54410001   {0, 4096, 0}
ATAG_MEM      0x54410002   {0x40000000, 0x00000000}   /* DDR0: 1 GiB @ 0 */
ATAG_MEM      0x54410002   {0x10000000, 0x80000000}   /* DDR1: 256 MiB @ 0x80000000 */
ATAG_CMDLINE  0x54410009   "console=ttyAM0,115200 root=/dev/ram0 rw ..."
ATAG_INITRD2  0x54420005   {0x00700000, initrd_size}
ATAG_NONE     0x00000000
```

命令行（内存相关部分取自主内核 `nflasha2:/kemco.txt`；`mem=` 由上面的
ATAG_MEM 提供，不重复写进 cmdline）：

```
console=ttyAM0,115200 root=/dev/ram0 rw ignore_loglevel earlyprintk
initcall_debug ip=off log_buf_len=4M
memrsv=1M@0 memrsv=0x8000@0xF00000
memrsv=0x1A5E000@0x02E68000 memrsv=0xA6EB000@0x09915000
memrsv=704M@0x14000000:uc
memrsv=14M@0x80000000:uc memrsv=242M@0x80e00000:uc
```

`memrsv` 是整条启动链的关键，见 §4。

### 2.3 引导加载器桩

QEMU 在 `0xffff0000` 放一小段位置无关代码，按 ARM Linux 约定设置寄存器后
跳到内核入口：

```asm
mov r0, #0          @ 保留
ldr r2, =atags      @ r2 = 0x00010000
ldr r1, =machine    @ r1 = 0x11a
ldr pc, =kernel     @ 0x00108000
```

对应字面量 `{0xe3a00000, 0xe59f200c, 0xe59f1004, 0xe51ff004, target,
machine, atags}`。`r1=0x11a` 是内核的机器号；不匹配时 Sony 内核会走
未知机器分支。

`0xffff0000` 在原机是只读 BootROM，但 Linux 使用 high vectors 并会向
`0xffff0000` 拷贝向量页，因此 QEMU 侧把它改成可写 RAM（`vectors` 区域）。

## 3. 页表与内核 head 补丁

### 3.1 为什么需要预置页表

Sony 的 bootloader（EXBL）在跳入内核前已经建立初始页表。原厂内核的 head
代码并不负责建立 `PAGE_OFFSET` 段的第一个映射，而是直接使用它。
QEMU 没有 EXBL，若什么都不做，`setup_arch` 读 ATAGS 时会发生 Data Abort。

解决办法：在加载内核前，把一个 16 KiB 的 L1 页表写到物理 `0x00104000`
（即 `swapper_pg_dir` 的物理位置），其中：

```
pgd[0x600] = (0x00000000 & 0xfff00000) | 0x00011c0e
```

`0x600` 是 VA `0x60000000` 的 L1 索引；`0x00011c0e` 是 1 MiB section
描述符（present、section、AP、cacheable 等）。它只覆盖 `0x60000000`
开头的 1 MiB，刚好够内核 head 读取 ATAGS 和自身代码；其余映射由内核
head 自己建立。

QEMU 写入后会从 `0x00105800` 读回 `pgd[0x600]` 并打印，作为“写进去了”的
证据，而不是只相信 `address_space_write` 的返回值。

### 3.2 跳过页表清零

原厂 head 在 `0x60108068` 处有一条分支，进入清空 `swapper_pg_dir` 的循环，
会把 QEMU 预置的表清零。补丁把该字改为 `0xEA000004`：

```
0x60108068: b 0x60108080     @ 跳过清零循环
```

文件偏移 `0x68`（基址 `0x60108000`）。这是整个方案里最“脆”的一处：
它依赖 `swapper_pg_dir` 恰好在 `0x60104000`、且清零循环长度为 4 条指令。

### 3.3 ATAGS 指针修复

Sony 的 stext 在 `0x6011efa0` 调用一段早期初始化代码，该函数以
`mvn r2, #0` 结束，把 bootloader 传入的 `r2`（ATAGS 指针）覆盖了。
head 随后在 `0x60108028` 调 `__vet_atags`（`0x60108118`），看到
非法指针对齐后把 `r2` 清 0，导致 `__atags_pointer=0`、`meminfo.bank[0]`
为空，`prepare_page_table` 反而清掉预置页表。

补丁把 `0x60108028` 的 `bl __vet_atags` 改为 `bl 0x60495f40`，在
`0x60495f40`（内核镜像内的零填充）放入桩：

```asm
0x60495f40: ldr r2, [pc, #4]   @ r2 = 0x00010000
0x60495f44: b   __vet_atags
0x60495f4c: .word 0x00010000
```

脚本在写入前断言目标 16 字节全为 0、且 `0x60108028` 原本确实是指向
`__vet_atags` 的 `bl` 编码。

## 4. 内存模型

### 4.1 主内核的真实内存地图

A7M3 的物理地址空间不是全部可用 RAM。**主内核**的启动参数位于
`nflasha2:/kemco.txt`（FAT12，挂载为 `/setting`；1,901 字节，SHA-256
`83b32a57278ee640e27fb60cbeec28a9c74b6055f5fe1b2b728d1c053b5988c8`）。
`nflasha1/BOOT/KEMCO.TXT`（815 字节）属于 updater/recovery 启动对，
不能当主内核参数使用。完整内容见
[`firmware/FINDINGS.md`](firmware/FINDINGS.md) §3 与
[`docs/LIRO-AND-MEMORY.md`](docs/LIRO-AND-MEMORY.md) §1。

主内核 `kemco.txt` 的内存部分：

```
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

关键点：

- DDR0 是 1 GiB @ 0；
- **DDR1 是 256 MiB @ `0x80000000`**（不是 updater 的 1016 MiB）；
- DDR1 被 `memrsv=14M@0x80000000:uc` 和
  `memrsv=242M@0x80e00000:uc` 整段覆盖，均为 `uc`；
- 低端 `0x14000000..0x3FFFFFFF` 是 704 MiB `uc`，不是 updater 的
  54 MiB `0x20000000` 窗口；
- LIRO 的 `0x8bdc1200..0x8bdc13dc` 落在 DDR1 的 242 MiB `uc` 区间内。

QEMU 现在按这份地图建模：DDR0 1 GiB、DDR1 256 MiB、两个 `ATAG_MEM`，
cmdline 带上面全部 `memrsv`。启动日志：

```
arm_add_memory: 0 0x0 0x40000000
arm_add_memory: 1 0x80000000 0x10000000
high_memory: b0000000
Memory: 1024MB 256MB = 1280MB total
Memory: 104856k/104856k available, 1205864k reserved, 0K highmem
```

可用内存只有约 102 MiB，是因为 704 MiB + 256 MiB 的 `uc` 区间被
`memrsv` 保留；这与主内核参数一致。早期“512 MiB 设备窗口”的配置只是
可启动的简化近似，已被本模型取代。

### 4.2 关键函数

| 符号/地址 | 作用 |
|---|---|
| `parse_memrsv` `0x6049ad34` | 解析 `<size>@<addr>[:<type>]` |
| 区间登记函数 `0x6049d55c` | 把 `memrsv` 区间写入非缓存地址表；只接受掩码 `0x14819` 中的类型 |
| `arch_phys_to_noncache` `0x6011f2d0` | PA→非缓存 VA；低端走线性别名，DDR1 走 `table[1]` 分支 |
| `create_mapping` `0x60118578` | `bootmem_init` 为设备/非缓存区建立映射 |
| 非缓存地址表 `0x604e07e0` | 静态初值 `{0x40000000, 0x90000000, 0xa0000000, 0xb0000000, 0xffffffff}`，运行时由 `memrsv` 填充 |

**只有 `memrsv` 区间会进入这张表；ATAG/`mem=` 的内存 bank 不会。**
使用主内核参数后，客户机从 PA `0x004e07e0` 读回：

```
00000014 00000080 00000074 000000a0 ffffffff
```

即 `{0x14000000, 0x80000000, 0x74000000, 0xa0000000, 0xffffffff}`。
`arch_phys_to_noncache(0x8bdc1200)` 因此返回 `0xabdc1200`；LIRO 之前
129 条 `arch_phys_to_noncache` 报错全部消失。

`upm.ko` 会用 Backup 数据里的 `0x2002104c` 调 `arch_phys_to_noncache`，
期望得到 `0x8002104c` 去读 UPM 寄存器。新模型下 `table[0]=0x14000000`
仍满足 `<= 0x2002104c`，该转换结果不变，`upm` 正常初始化。

### 4.3 失败过的方案（保留，避免重复踩坑）

| 方案 | 结果 | 原因 |
|---|---|---|
| 1 GiB ATAG，不设 `memrsv` | `upm` NULL 解引用 | helper 只接受 end-of-DRAM（`0x40000000`）以上的地址，`0x2002104c` 被拒绝 |
| `vmlinux-noncache.bin`：把 `0x6011f370` 的 `cmp r1,r2` 改成 `cmp r2,r1` | `upm` 加载，但语义错误 | 让 helper 接受任意 DRAM 地址并返回线性别名，读到的是 RAM 而非寄存器 |
| 512 MiB + QEMU 预置窗口 + `vmlinux-window.bin` 跳过 `paging_init` 清零 | 仍 NULL | `paging_init` 清掉窗口映射 |
| 512 MiB + `memrsv=0x20000000@0x20000000:device` | 内核 panic | 设备区与 memblock 内存不重叠 → `device attribute area is in secgrp` |
| **1 GiB ATAG + `memrsv=...:device`（历史简化模型）** | 通过（已被取代） | 设备区与 memblock 重叠，`bootmem_init` 建立非缓存映射；但 LIRO 的 `0x8bdc1200` 仍被拒绝 |
| DDR1 1016 MiB @ `0x80000000` + 旧 cmdline | LIRO 仍报 `0x8bdc1200` | 1016 MiB 是 updater 参数；且没有 `memrsv` 登记 DDR1，`table[1]` 仍为 `0x90000000` |
| **DDR1 256 MiB + 主内核 `memrsv`（当前方案）** | 通过 | `table[1]=0x80000000`，`0x8bdc1200 → 0xabdc1200`；非缓存报错消失 |

`experimental/patch-noncache-1g.py` 与 `experimental/patch-window-512m.py`
保留了前两个失败实验的补丁脚本，仅作记录，不建议使用。

### 4.4 当前配置（主内核真实地图）

- QEMU DDR0：`0x00000000`，1 GiB。
- QEMU DDR1：`0x80000000`，256 MiB。
- ATAG_MEM：1 GiB @ 0 + 256 MiB @ `0x80000000`。
- cmdline 内存部分：`memrsv=1M@0`、`memrsv=0x8000@0xF00000`、
  `memrsv=0x1A5E000@0x02E68000`、`memrsv=0xA6EB000@0x09915000`、
  `memrsv=704M@0x14000000:uc`、`memrsv=14M@0x80000000:uc`、
  `memrsv=242M@0x80e00000:uc`。
- 非缓存表运行值：`{0x14000000, 0x80000000, 0x74000000, 0xa0000000, 0xffffffff}`。
- 可用普通内存约 102 MiB（`uc` 区间被保留），与主内核参数一致。

注意：本模型只复现了 `memrsv` 对非缓存地址表/保留区的语义，并未模拟
`uc` 区间的实际缓存策略、`wbi_memclr`/`wbimem`、`pm.*` 等其余 Sony
启动参数。LIRO 的 `0xf2a1xxxx` 硬件块仍未建模（见 §12）。

## 5. 串口控制台

PL011 在 QEMU 中工作正常，但原厂内核不会向它写数据。原因不在 PL011 模型：

- `pl011_console_setup` 从 Sony 的 `amba_ports+4` 复制“控制台已使能”标志；
- 该标志只由 `em_amba2_console_enable` 设置；
- 后者依赖 `errind_console_enable_triggered` 读取板上的 ADC/strap；
- QEMU 没有这个 ADC，标志恒为 0，`pl011_console_write` 直接返回，
  连一个字节都不写 DR。

调试时观察到：内核确实在写 PL011 的 CR/IMSC/ICR，但 `UARTDR` 的写次数为 0
——这是“门控在写路径里，而不是中断/时钟问题”的直接证据。

补丁在 `0x602988cc` 把 `str r3, [r6, #0xd4]`（`0xE58630D4`）改成
`str r2, [r6, #0xd4]`（`0xE58620D4`）。该点 `r2` 已经是 1，于是
“控制台已使能”标志恒为 1。之后串口立刻出现完整内核日志。

注意：这是**强制打开**，不代表 Sony 的 errind 状态机被正确模拟。
如果以后模拟了 ADC/strap，应移除该补丁。

## 6. eMMC 与分区

### 6.1 QEMU SD 模型布局

ma1co/qemu 的 SDHCI 模型把 eMMC 的 user area 放在 `2 * boot_size` 处：

- `boot_size = 0x40000`（256 KiB），两个 boot 分区；
- 文件前缀 `0x80000` = 两个 boot 分区；
- user area 从文件偏移 `0x80000` 开始，2 GiB。

因此 MBR 位于文件偏移 `0x80000`，不是文件开头。构建脚本对每个 `dd`
都使用 `conv=sparse,notrunc`，否则后续写入会截断 2 GiB 镜像。

### 6.2 MBR 布局

`sfdisk` 生成 DOS 分区表：1–3 为主分区，4 为扩展容器（type 0x05），
5–25 为逻辑分区，大小取自服务模式转储的 `MANIFEST.md`。关键点：

| 分区 | 内容 | 用途 |
|---|---|---|
| `nflasha2` | vfat | `/setting` |
| `nflasha3` | FAT12 | `/system`，含 `VMLINUX.BIN`、`INITRD.IMG` |
| `nflasha7` | ext2 | 真正的相机 rootfs（`/rootfs`，切换后为 `/`） |
| `nflasha10` | vfat | `/tmp` 与 `/etc` |
| `nflasha11` | vfat | `/log` |
| `nflasha15` | ext2 | `/usr`，含 `im.elf`、`grm_gles.ko`、`upm.ko` 等 |
| `nflasha18` | vfat | `/lens` |
| `nflasha12` | vfat | `/cert` |

`nflashaB0/B1` 来自两个 boot 分区，由 QEMU 模型自动暴露。

分区表由 MBR 提供后，内核的 `emmc.ko` 创建 `nflasha1..25`。真机 eMMC 上
还存在 Sony 的 `ptbl`/ALFReD 信息表，本实验没有重建，因此 `dnflasha`/WBI
路径不存在；这直接影响 `backup.ko`（见 §8.1）。

启动日志会先出现 `mounting1 /dev/dnflasha7 -> /rootfs as ext2 failed. errno(6)`
和 `/dev/dnflasha15 -> /usr` 的同类失败，然后回退到 `nflasha7`/`nflasha15`。
这是预期行为，不是挂载失败。

### 6.3 rootfs 的 ext2 偏移

`nflasha15` 是 1024 字节块的 ext2。`grm_gles.ko` 的 inode 为 40880，
数据块为 `173897-173908`（直接）、`173909`（间接）、`173910-173913`；
`init_module` 位于文件偏移 `0x1944` = 块内偏移 324。

镜像中的写入偏移：

```
0x80000 + 1531904 * 512 + 173903 * 1024 + 324
```

`1531904` 是 `nflasha15` 的起始 LBA（由上面的 sfdisk 布局决定）。
这些常量只对当前分区表和 rootfs 镜像成立；脚本会先读 8 字节并比对
ARM 函数序言 `0dc0a0e1 f0d82de9`，不匹配就拒绝写入。

## 7. initrd 与用户态

### 7.1 initrd 是 ext2 镜像

`initrd.img`（2,178,048 字节）不是 cpio，而是一个 **ext2 文件系统镜像**，
由内核作为 `root=/dev/ram0` 挂载。内核日志也直接说明了这一点：

```
rootfs image is not initramfs (junk in compressed archive); looks like an initrd
Freeing initrd memory: 2124K
```

其根目录包含：

```
/sbin/init     28,572 B   相机 init（模块列表和参数嵌入在二进制里）
/kmod/...                 早期模块
/rootfs/                  挂载点（空）
/lib/ /dev/ /proc/ ...
```

`/sbin/init` 负责挂载 `nflasha*`，把 `nflasha7` 挂到 `/rootfs` 并切换到
真正的相机 rootfs。最终可交互的 BusyBox（`/bin/ash` 等）来自 `nflasha7`，
不是 initrd。

### 7.2 initrd 补丁

模块列表是 `/sbin/init` 里的 ASCII 字符串，因此可以直接在 ext2 镜像里
做**等长**替换（ext2 无校验和，长度不变就不影响元数据）：

| 偏移 | 原文 | 改为 | 原因 |
|---|---|---|---|
| `2,016,076` | `Backup_load=0 ` | `Backup_load=1 ` | 走文件加载路径，见 §8.1 |
| `2,016,172` | `/usr/kmod/udate.ko` | `/usr/kmod/udate.zz` | 跳过会 NULL 解引用的模块 |

这两处字符串在 `/sbin/init` 文件内的偏移分别是 `12,108` 和 `12,204`；
镜像偏移由上表给出。`patch-initrd.py` 断言每个字符串恰好出现一次，并在
写后读回校验。

### 7.3 Sony 异常监视器

Appframework 启动失败后，Sony 的 exception monitor 接管串口，提示符为
`Exception>`。它不是 QEMU 或内核 panic。观察到可用命令：

```
exit      返回 BusyBox shell（打印 Good Bye.）
shell     /bin/sh
reg       寄存器
stack     栈
map       映射
modules   模块列表
reboot    重启
```

自动化测试看到 `Exception>` 后应输入 `exit`，等待 `/ #`。

## 8. 模块与驱动

### 8.1 backup.ko

`backup.ko` 以 `Backup_load=0` 启动时返回 `-EIO`。真机上 backup 数据来自
WBI/`dnflasha` 路径；本实验没有该路径，因此改为 `Backup_load=1`，让它走
文件加载路径。这是**改变行为**的补丁，不是模拟；
如果以后重建了 `ptbl`/WBI，应恢复 `Backup_load=0`。

### 8.2 udate.ko

`udate_init_clock` 在 QEMU 中 NULL 解引用（UDIF 时钟不存在）。直接把
模块路径改成不存在的等长路径，让 init 跳过加载。副作用是依赖 udate 的
时间/更新功能不可用，但 shell 和 Appframework 启动不受影响。

### 8.3 grm_gles.ko

原版在 `sugi_hw.c:221` 触发 `BUG()`：Sugilite GPU 时钟在 QEMU 中永远不会
就绪，`init_module` 在 pid 1 的上下文里执行，BUG 直接杀死 init。

补丁把 `init_module` 开头 8 字节替换为：

```
mov r0, #0     @ 0xe3a00000
bx  lr         @ 0xe12fff1e
```

字节序后为 `0000a0e3 1eff2fe1`。模块加载返回 0，但**不会注册字符设备**
（也没有 `/dev/dmpgles2`），也不会初始化 GPU。

实验性的替代方案（`scripts/experimental/patch-grmgles-clock.py`）：恢复原始
`init_module`，只把 `sugilite_clock_up_internal` 结尾的
`cmp r0,#0x15; ldmeq` 改成 `mov r0,#0; ldm`。这样模块 init 会继续并注册
**`dmpgles2`（major 248, minor 29）**；节点仍需用
`add-dmpgles2-node.py` 离线加入。这是硬件-free 的绕过，不是 GPU 模型。

### 8.4 其他

- `dma330` 初始化会停顿约 10 秒，属正常现象，不要误判为死机。
- `dmm.ko`、`upm.ko` 在内存模型正确后均可初始化。
- `emmc.ko` 接受 MBR 分区表并创建 `nflasha*`；`ptbl` 缺失只会导致
  `dnflasha`/WBI 相关路径不可用。

### 8.5 DFB / `nflasha4`（Appframework 崩溃的直接原因）

`libmpr.so`（4.03 运行时，SHA-256
`ab1df757108d123f403188e33b732bf0c8cded33ff29711417dca14c5ab883a5`）
的 DFB 初始化会打开 `/dev/nflasha4`。合成 MBR 把 `nflasha4` 做成只有
1 个扇区的扩展容器，`DFA::FlashWrite` 的 `lseek` 失败；错误路径在已持有
MemInfo 互斥量的情况下再次加锁，触发 glibc 2.11.2
`pthread_mutex_lock.c:62` 断言。

实验性绕过：把 DFB 打开路径改为 `/dev/nflasha16`（140 MiB 全零分区），
共 3 处字节改动，详见
[`docs/LIRO-AND-MEMORY.md`](docs/LIRO-AND-MEMORY.md) §4 和
`scripts/experimental/patch-libmpr-dfb16.py`。补丁后 `im.elf` 可稳定
运行到 63 线程。这是绕过，不是 DFB/CMMeX 的真实模拟。

## 9. Appframework 崩溃路径

init 在模块加载完成后启动 `/usr/bin/im.elf`（Appframework）。历史顺序：

1. 默认 `grm_gles.ko` 打桩/原始 `init_module` 时，`/dev/dmpgles2` 不存在，
   `FixedArea` mmap 失败，进程 abort；
2. 使用实验性时钟绕过 + `/dev/dmpgles2` 节点后，`FixedAreaTop=20098000`、
   `FixedAreaSize=0007C000`，Appframework 打开并 mmap 128 KiB；
3. 随后在 `THREAD_INFRA1` 观察到 `libmpr.so` 内的互斥量/`MemInfo` 崩溃
   （早先符号化为 `__pthread_mutex_lock(NULL+0x1c)`）。

进一步反汇编表明，直接原因是 **DFB 在 1 扇区的 `nflasha4` 上写失败后的
错误路径二次加锁**（见 §8.5）。把 DFB 目标改到 `/dev/nflasha16` 后：

- 不再出现 `pthread_mutex_lock` 断言/trim 错误；
- `im.elf` 稳定运行，`Threads: 63`；
- `libmpr.so`/`libmprctrl.so`/`libmprdummy.so` 已映射；
- `viewUnified8.so`/`libgrm.so`/`libuxengine.so`/`libDDS.so` 仍未加载；
- `bootin.elf` 3 线程，wchan `osal_rcv_msg_tmo`（UIPC 等待）。

因此设备打开、FixedArea 和 DFB 都已不是当前首要阻塞；UI 阶段还缺
UIPC/CP 握手与显示模型。LIRO 固件存在时的 RT 忙等见 §12。

## 10. 与 A7C II 工作的差异

| 项目 | A7M3 | A7C II |
|---|---|---|
| SoC | CXD90045 | CXD90057 |
| 架构 | ARM32（Cortex-A5） | AArch64 |
| 内核 | 3.0.27_nl-rt106+ | 更新版本 |
| UI/GPU | Sugilite（`grm_gles.ko`） | 不同的图形栈 |
| 启动协议 | ATAGS + 机器号 0x11a | 不适用 |

A7C2 的 AArch64 runtime 不能直接复用到 A7M3；本目录是独立的 ARM32 启动链。

## 11. 常量表

| 名称 | 值 |
|---|---|
| 机器号 | `0x11a` |
| 内核链接基址 | `0x60108000` |
| `PAGE_OFFSET` | `0x60000000` |
| `TEXT_OFFSET` | `0x108000` |
| 内核加载 PA | `0x00108000` |
| initrd 加载 PA | `0x00700000` |
| ATAGS PA | `0x00010000` |
| 初始页表 PA | `0x00104000` |
| `pgd[0x600]` | `0x00011c0e` |
| DDR0 | `0x00000000` + 1 GiB |
| DDR1 | `0x80000000` + 256 MiB |
| 非缓存地址表 | 内核 VA `0x604e07e0`（PA `0x004e07e0`） |
| 非缓存表运行值 | `{0x14000000, 0x80000000, 0x74000000, 0xa0000000, 0xffffffff}` |
| `memrsv`（内存部分） | `1M@0`, `0x8000@0xF00000`, `0x1A5E000@0x02E68000`, `0xA6EB000@0x09915000`, `704M@0x14000000:uc`, `14M@0x80000000:uc`, `242M@0x80e00000:uc` |
| LIRO 固件 | 18,449,784 B；加载 PA `0x035c6000`；入口 `0x635c6044`/`0x635c6058`；忙等 PC `0x63a1d294` |
| eMMC user area 偏移 | `0x80000` |
| `nflasha15` 起始 LBA | `1531904` |
| `grm_gles.ko` inode | `40880` |
| `init_module` 文件偏移 | `0x1944` |

## 12. LIRO 协处理器固件（2026-10-31 晚）

LIRO 固件 `nflasha3:/AV-CAM.BIN`（4.03，18,449,784 字节，SHA-256
`e1b0f84e8323005a34b43de6dd0083fca712d2c654f0d9716512e0a54631bbe7`）
由 bootloader/QEMU loader 放到 PA `0x035c6000`（VA `0x635c6000`），
`liro.ko` 解析头部后执行：

```
entry_pre  = 0x635c6044
entry_main = 0x635c6058
```

本地日志中两者均返回。任务遍历（在 `entry_main` 返回后执行）看到至少
134 个 `liro-kliro_*` 内核线程（另有 `udm_liro`）。但随后串口完全静默；
GDB 中断显示当前 CPU 在 LIRO 固件的 Thumb 循环里：

```
63a1d28e: ldr r0, [pc, #0x16c]   ; 0xf2a1102c
63a1d290: bl  0x63d6c97a         ; r0 = *(u32 *)r0
63a1d294: tst.w r0, #2
63a1d298: beq 0x63a1d28e
```

QEMU 的 `cxd90045` 机器没有建模 `0xf2a1xxxx` 块，读回 0，于是固件在
RT 线程中无限忙等，抢死 console、shell 和 Appframework。这是目前
“原生 UI 之前”的主要阻塞。

已做的负实验：把 `0x63a1d298` 的 `beq` 改成 NOP（文件偏移 `0x457298`）
后 `entry_main` 仍返回，但系统依旧静默，说明后面还有同类等待。脚本
`scripts/experimental/patch-liro-nowait.py` 仅用于离线 QEMU，严禁用于真机。

不预加载固件时，`liro.ko` 打印 `Magic unmatched 0 != 4c49524f`，系统继续
启动，`im.elf` 运行到 63 线程——可见静默是固件忙等造成的，不是新内存
模型或应用本身的问题。

下一步：给 `0xf2a1xxxx` 做最小寄存器模型（先让 `+0x2c` 返回 bit1），
并继续定位后续等待点；详见
[`docs/LIRO-AND-MEMORY.md`](docs/LIRO-AND-MEMORY.md) §5、§8。
