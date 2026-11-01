# ILCE-7M3（A7M3）固件 QEMU 启动尝试总结

> **阅读说明（整理进固件包时添加）**
> 本文是 2026-10-31 的实验记录，保留当时的推理过程；部分中间结论已被
> 后续工作修正。最新结论以 [`FINDINGS.md`](FINDINGS.md)、
> [`MANIFEST.md`](MANIFEST.md) 和公开仓库的 `../STATUS.md` 为准。
>
> 主要修正：
> - 最终 QEMU 模型：DDR0 PA 为 `0x00000000`，`PAGE_OFFSET=0x60000000`；
>   §13.2 的 `DDR0_BASE=0x60000000` 是中间实验。
> - initrd 加载地址是 PA `0x00700000`（§14.1 原句的 `0x01000000` 是笔误）。
> - 主内核参数已定位到 `nflasha2:/kemco.txt`（不是
>   `nflasha1/BOOT/KEMCO.TXT`）：DDR1 256 MiB，`memrsv` 含
>   704M/14M/242M `uc`；QEMU 已按此建模。详见 `FINDINGS.md` §3.0 与
>   `../docs/LIRO-AND-MEMORY.md`。
> - `backup.ko` 已用 `Backup_load=1` 解决，`upm.ko` 已用 `memrsv` 解决；
>   Appframework 的设备节点是 `/dev/dmpgles2`（248:29）。后续确认崩溃根因是
>   DFB 在 1 扇区 `nflasha4` 上写失败后的二次加锁；改到 `/dev/nflasha16`
>   后 `im.elf` 可运行到 63 线程。当前主阻塞是 LIRO/CP 的 `0xf2a1xxxx`
>   硬件忙等。
> - 设备序列号已隐去。

- 日期：2026-10-31
- 状态：**已启动到相机用户的 BusyBox 根 shell，串口控制台可用**（见第 14 节；早期“阻塞在 paging_init”的结论已被第 14 节取代）
- 关联成果：仓库内 `report/2026-10-17-qemu-native-camera/`（A7C II，已能跑原生菜单）
- 本文只记录 A7M3 方向的实测结果、证据和困难；Sony 固件本身不随文档分发。

---

## 1. 结论先行

A7M3 固件**不能直接替换**当前 A7C II 的 `start.sh` runtime：

| 项目 | A7C II（当前可运行） | A7M3（本次尝试） |
|---|---|---|
| SoC | CXD90057 | **CXD90045** |
| CPU 架构 | **AArch64**（`cortex-a35`） | **ARM32**（`cortex-a5`） |
| 内核 | 4.9.x | **Linux 3.0.27_nl-rt106+（Sony CE Linux）** |
| 用户态 | AArch64 ELF | **ELF32 ARM EABI5** |
| initrd 库 | glibc 2.25 | **glibc 2.11.2** |
| 固件 FDAT model | `0x20030014` | **`0x71030014`** |
| 当前 runtime | 已导入、菜单可用 | **QEMU 已能启动到相机用户态 BusyBox 根 shell（无 UI）** |

用 ma1co/qemu 的 `cxd90045` 机器现在可以**完整启动** A7M3 内核与相机用户态：
初始化脚本完成全部 `/dev/nflashaN` 挂载、加载内核模块，并落到 BusyBox ash。
串口控制台可用；Sony 的异常监视器出现时输入 `exit` 即回到该 shell。
详见第 14 节。UI 应用 `im.elf` 仍会因缺少 Sony FixedArea/硬件而在启动后崩溃。

---

## 2. 本次环境

- Windows 11 + WSL2 Ubuntu 24.04（x86_64）
- WSL 内存上限 28 GB（A7C II 的 QEMU 需要 24 GiB 客户机内存）
- 工作路径（均用 `$HOME` 表示，避免机器专属绝对路径）：
  - `$HOME/firmware/a7m3/`：A7M3 更新包、解包结果、日志
  - `$HOME/fwtool-exe-fix/`：支持新 EXE 格式的 fwtool 分支
  - `$HOME/qemu-ma1co/`：ma1co/qemu 5.1.0 源码与 `build-arm` 构建目录
- 本次未改动 A7C II 的 `$HOME/a7c2-re` 工作副本。

---

## 3. 固件获取与解包

### 3.1 官方包

- 文件名：`Update_ILCE7M3V404.exe`
- 版本：4.04
- 大小：`339,275,208` 字节
- SHA-256：`8c356688a70946a855191e99fd4d9096a5c3d4263f3f11065605eaea45639d57`
- 来源：Sony 官方 `support.d-imaging.sony.co.jp`（通过官方支持页面的下载链接取得）

### 3.2 新 EXE 格式

4.04 不再是旧版 LZH 自解压格式。包尾是一个 Sony Package 容器：

- 内含 `Settings.ini` 与一个 `.cab`；
- CAB 的全部字节先与 `0xff` 异或，再使用 LZX 压缩；
- 上游 fwtool master 的 `unpackInstaller()` 会报 `Unknown exe file`。

解决方式：使用 DavidBuchanan314 的 **fwtool PR #53** 分支 `exe-fix`（含纯 Python LZX 解压器和 `InvertedFile`）。该 PR 在撰写时仍是 open，未合入 master。

解包命令：

```bash
cd "$HOME/fwtool-exe-fix"
python3 fwtool.py unpack -f "$HOME/firmware/a7m3/Update_ILCE7M3V404.exe" \
  -o "$HOME/firmware/a7m3/4.04-unpacked"
```

### 3.3 解包结果

```text
$HOME/firmware/a7m3/4.04-unpacked/
├── config.yaml
├── firmware.dat             335,395,240 B
├── firmware.fdat            334,084,096 B
├── firmware.tar             333,936,128 B
├── firmware.tar_unpacked/
├── updater.img                  147,456 B
└── updater.img_unpacked/
```

`config.yaml` 关键内容：

```yaml
dat:
  crypterName: CXD90045
fdat:
  isAccessory: false
  model: 0x71030014
  region: 0
  version: '4.04'
```

### 3.4 关键分区与文件

`firmware.tar_unpacked/0700_part_image/dev/` 下：

| 文件 | 大小（字节） | 说明 |
|---|---:|---|
| `nflasha3` | 31,493,120 | 内核、initrd、CP/loader 文件 |
| `nflasha5` | 31,448,352 | 另一分区 |
| `nflasha7` | 5,123,072 | 已解包出文件树 |
| `nflasha15` | 217,267,200 | `/usr` 等用户态 |
| `nflasha3_unpacked/vmlinux.bin` | 4,117,760 | Linux 内核（raw ARM Image） |
| `nflasha3_unpacked/initrd.img` | 2,178,048 | ext2 格式 initrd |

`initrd.img_unpacked/` 中包含：

- `sbin/init`：ELF32 ARM，glibc 2.11.2；
- `kmod/`：`emmc.ko`、`nand.ko`、`ldec.ko` 等；
- `version.txt`：`37114_13.2026020901`。

### 3.5 内核与用户态架构

`file` / `readelf` 实测：

- `vmlinux.bin`：不是 zImage，也不是 ELF；开头即 ARM32 启动代码（`msr CPSR_c, ...`），是原始 `Image`。
- `nflasha15_unpacked/lib/*.so`：全部为 **ELF 32-bit LSB, ARM, EABI5**。
- 内核版本字符串：

```text
Linux version 3.0.27_nl-rt106+ (jp10239@dic-build-scm-u20-009)
(gcc version 4.5.1 (20220614 (Sony CE Linux 8.2.7.6)))
#1 SMP PREEMPT RT Mon Feb 9 15:18:43 JST 2026
```

这与 A7C II 的 AArch64/glibc 2.25 完全不同，是“不能直接套用现有 runtime”的根本原因。

---

## 4. 为什么不能直接用当前 A7C II runtime

当前 `2026-10-17-qemu-native-camera` 的启动链按 CXD90057 写死：

| 位置 | 固定内容 |
|---|---|
| `runtime/analysis/prepare-assets.py` | 输入固定为 `nflasha3/8/15`、固定大小/SHA；FDAT model 固定 `0x20030014` |
| `runtime/analysis/qemu-kernel-probe.sh` | DDR 基址 `0x400000000`、`-m 24576`、内核装载 `0x400080000`、initrd `0x440000000` |
| 同上 | GIC/SCU/timer/GPIO 的固定 VA 别名（如 `0xfffffffff1042000`） |
| `source/qemu-sony-native.patch` | timer 模型注释明确为 CXD90057 |
| `build-qemu.sh` | 只构建 `aarch64-softmmu,aarch64-linux-user` |
| guest observer/IMDB 补丁 | 全部按 A7C II 的 AArch64 库与符号编写 |

A7M3 需要的是 **ARM32 的 `arm-softmmu`**、CXD90045 板级、新的内存映射、新的 `prepare-assets` 以及 32 位 guest helper；这些都不是“替换一个 `.so`”能解决的。

---

## 5. ma1co/qemu cxd90045 机器

### 5.1 源码与构建

- 仓库：`https://github.com/ma1co/qemu`
- 版本：QEMU 5.1.0（fork 内 `VERSION`）
- `default-configs/arm-softmmu.mak` 中 `CONFIG_BIONZ=y`
- `hw/arm/bionz.c` 已有：

```c
DEFINE_MACHINE("cxd90045", cxd90045_machine_init)
mc->desc = "Sony BIONZ CXD90045";
mc->default_cpu_type = "cortex-a5";
```

CXD90045 常量（来自 `hw/arm/bionz.c`）：

```c
DDR0_BASE 0x00000000  DDR0_SIZE 0x40000000
DDR1_BASE 0x80000000  DDR1_SIZE 0x40000000
SRAM_BASE 0xfe000000   SRAM_SIZE 0x01000000
UART_BASE(i) 0xf2000000 + i*0x1000
HWTIMER_BASE(i) 0xf2403000 + i*0x100
MPCORE_BASE 0xf8000000
TEXT_OFFSET  0x00108000
INITRD_OFFSET 0x00700000
```

构建命令：

```bash
cd "$HOME/qemu-ma1co"
mkdir -p build-arm && cd build-arm
../configure --target-list=arm-softmmu --disable-werror --disable-docs --disable-tools \
  --disable-gtk --disable-sdl --disable-opengl --disable-seccomp --disable-capstone \
  --disable-virtfs --disable-guest-agent --disable-usb-redir --disable-libiscsi \
  --disable-libnfs --disable-glusterfs --disable-rdma --disable-vnc --disable-spice \
  --disable-curl --disable-bzip2
make -j8
```

产物：`build-arm/arm-softmmu/qemu-system-arm`。

额外依赖：`libmad0-dev`（configure 默认要求 libmad），以及 QEMU 5.1 构建时会自动拉取 dtc/slirp/softfloat/keycodemapdb 子模块。

### 5.2 为启动 A7M3 做的本地修改

以下修改只在 `$HOME/qemu-ma1co` 的本地副本中，未进入公开仓库。

#### 修改 1：解决 serial0 争用

原 `cxd90045_init()` 中 `bionz_bootcon` 与第一个 PL011 同时使用 `serial_hd(0)`，在 QEMU 5.1 的 chardev 模型下会 abort：

```text
Property 'pl011.chardev' can't take value 'serial0': Device 'serial0' is in use
```

处理：移除 `bionz_bootcon` 的 `qdev_prop_set_chr(dev, "chardev", serial_hd(0));`，把 `serial0` 留给内核控制台 UART。

#### 修改 2：启动 stub 设置 ARM Linux 引导寄存器

原 stub 只有 `ldr pc, [pc, #-4]`，不设置 `r0/r1/r2`。A7M3 内核走标准 ARM32 Linux 引导协议，需要：

- `r0 = 0`
- `r1 = machine ID`
- `r2 = ATAGS 物理地址`

machine ID 从内核 `machine_desc` 静态解析得到：

- `vmlinux-to-elf` 重建符号后，内核基址为 `0x60108000`；
- `machine_desc` 名称字符串 `"ARM-CXD900X0"` 位于文件偏移 `0x34ad9b`，VA `0x60452d9b`；
- 对应 `machine_desc` 结构在文件偏移 `0x3b0ce4`，`nr = 0x11a`（十进制 282）。

启动 stub 改为：

```c
uint32_t loader[] = {
    0xe3a00000, /* mov r0, #0 */
    0xe59f200c, /* ldr r2, [pc, #12] */
    0xe59f1004, /* ldr r1, [pc, #4] */
    0xe51ff004, /* ldr pc, [pc, #-4] */
    target,
    machine,    /* 0x11a */
    atags,
};
```

#### 修改 3：构造最小 ATAGS

在 `cxd90045_init()` 中把 ATAGS 写到物理地址 `0x10000`，内容包括：

- `ATAG_CORE`（`0x54410001`）
- 两个 `ATAG_MEM`（`0x54410002`）：
  - DDR0：`0x00000000`，`0x40000000`
  - DDR1：`0x80000000`，`0x40000000`
- `ATAG_CMDLINE`（`0x54410009`）
- `ATAG_INITRD2`（`0x54410005`）：`0x00700000`，initrd 文件大小
- `ATAG_NONE`

cmdline 初始为：

```text
console=ttyAMA0,115200 root=/dev/ram0 rw ignore_loglevel
```

后续又尝试追加：

```text
memrsv=0x4000@0x104000 memrsv=0x400000@0x108000 memrsv=0x1000@0x10000
```

结果：故障点不变。

---

## 6. 启动试验与证据

### 6.1 启动命令

```bash
qemu-system-arm -M cxd90045 \
  -kernel "$HOME/firmware/a7m3/4.04-unpacked/firmware.tar_unpacked/0700_part_image/dev/nflasha3_unpacked/vmlinux.bin" \
  -initrd "$HOME/firmware/a7m3/4.04-unpacked/firmware.tar_unpacked/0700_part_image/dev/nflasha3_unpacked/initrd.img" \
  -display none -serial file:"$HOME/firmware/a7m3/serial.log" -no-reboot
```

串口文件始终为 0 字节。

### 6.2 执行轨迹

用 `-d in_asm,cpu_reset,int,guest_errors,unimp` 记录，并用 `vmlinux-to-elf` 重建符号：

1. CPU 从 `0xffff0000` 执行启动 stub；
2. 跳到 `0x00108000`，执行 ARM32 `head.S`；
3. 打开 MMU，运行到内核 VA；
4. 执行 `cpu_init`（`0x60110010` 附近）；
5. 进入 `paging_init`（`0x6049cxxx`）；
6. 第一次 Data Abort：

```text
Taking exception 4 [Data Abort]
...from EL1 to EL1
...with ESR 0x25/0x9600007f
...with DFSR 0x805 DFAR 0x60105c00
```

7. 随后异常向量页未映射，进入 Prefetch Abort 循环：

```text
Taking exception 3 [Prefetch Abort]
...with IFSR 0x5 IFAR 0xffff0010
...with IFSR 0x5 IFAR 0xffff000c
```

8. 通过 QMP 读取物理内存：

```text
pgd @ 0x104000: 0x00000000 ...
```

即内核的顶层页表页已经被清零；随后对 `0x60105c00`（pgd 页内的一个表项地址）的写入触发翻译错误，而 high vectors 又未建立，于是无限 abort。

### 6.3 已尝试但无效的修补

- 增加 `ATAG_MEM`（DDR0/DDR1 各 1 GiB）
- 增加 `memrsv`（保护 pgd 区、内核区、ATAGS 区）
- 调整 cmdline（console、root、initrd）
- 修正 serial0 争用
- 设置 machine ID `0x11a`

以上修改后，内核仍能执行到 `paging_init`，但故障点相同，串口仍无输出。

---

## 7. 根因判断

当前证据支持以下判断，而不是“固件解包错误”：

1. **A7M3 更新包不包含 eMMC boot 分区**。
   它只含 `nflasha3/5/7/8/15` 等应用/系统分区，没有 `EXBL` boot block。
   ma1co 的 `cxd90045_init_loader2()` 正是从 eMMC 读取 `EXBL` 签名、加载 boot 分区并跳转到 Sony 自己的 loader；这条路径才可能完成原始内存/页表准备。

2. **裸 `-kernel` 启动缺少 Sony bootloader 的启动参数**。
   A7M3 内核在 `paging_init` 中会重建/清零页表；当前没有原始 bootloader 传入的内存预留和启动参数，内核把自己的 pgd 自映射清掉后访问 pgd 页，触发 Data Abort。

3. **这不是“换个机器名”能解决的**。
   需要的是：
   - A7M3 的 eMMC boot 分区/整机 dump；或
   - 逆向 Sony 内核在 `paging_init` 之前期望的 boot params/memrsv 语义，并在 QEMU 中完整模拟。

在拿到上述任一条件之前，无法声称 A7M3 能在 QEMU 中启动到串口。

---

## 8. 后续可选路线

### 路线 A：使用原始 eMMC boot 分区（推荐）

- 取得 A7M3 的 eMMC boot0/boot1 或整机 dump；
- 用 ma1co 的 `EXBL` 路径启动，而不是 `-kernel`；
- 成功判据：Sony loader 运行、内核串口输出、进入 initrd。

### 路线 B：继续逆向 `paging_init` 前的启动协议

- 从 `machine_desc`、`memblock`、`arm_memblock_init`、`paging_init` 入手；
- 确认内核期望的 memory map、pgd 位置、memrsv 语义；
- 在 QEMU 中补齐 boot params；
- 成功判据：Data Abort 消失、串口出现内核日志。

### 路线 C：菜单级移植（远期）

即使内核启动成功，A7M3 原生菜单还需要：

- ARM32 交叉编译的 observer/预加载库；
- A7M3 的 IMDB/服务/输入/图形逆向；
- 新的 `prepare-assets` 和 runtime 布局；
- 32 位 QEMU 显示/输入桥接。

这是独立的研究项目，工作量以月计。

---

## 9. 复现所需的本地路径

```text
$HOME/firmware/a7m3/Update_ILCE7M3V404.exe     官方更新包（不随文档分发）
$HOME/firmware/a7m3/4.04-unpacked/             fwtool 解包结果
$HOME/firmware/a7m3/serial*.log                串口日志（本次为空）
$HOME/firmware/a7m3/trace.log                  in_asm 轨迹
$HOME/firmware/a7m3/int.log                    异常/中断日志
$HOME/firmware/a7m3/fault.log                  首次 Data Abort 前的指令轨迹
$HOME/firmware/a7m3/a7m3-vmlinux.elf           vmlinux-to-elf 重建的带符号 ELF
$HOME/fwtool-exe-fix/                          fwtool PR #53 分支
$HOME/qemu-ma1co/                              ma1co/qemu 源码与 build-arm
```

---

## 10. 重要限制

- Sony 固件、解包树、QEMU 二进制均为本机实验产物，**不提交、不分发**。
- 本文的结论只针对“ILCE-7M3 4.04 + ma1co/qemu cxd90045 + 当前裸 `-kernel` 启动方式”；
  不能据此断言 A7M3 永远无法在 QEMU 中运行，只能说明**当前捷径不足**。
- 本次修改的是 ma1co/qemu 的本地副本；上游代码和 A7C II 成果均未被改写。

---

## 11. 服务模式只读探测（2026-10-31 续）

本节记录对**实机 ILCE-7M3** 的服务模式（Senser）只读探测。所有操作只发送查询/读取类请求，
未写入、未删除、未刷写任何分区；相机侧 `libJiritsu` 在处理读取请求时**自行尝试**挂载
`/dev/nflasha6`，该挂载在本机返回失败。

### 11.1 设备与入口

| 项 | 值 |
|---|---|
| 机型 | ILCE-7M3（序列号已隐去） |
| 普通海量存储身份 | `054c:0c02`（bInterfaceClass 08/06/50，SCSI transparent） |
| 服务模式身份 | `054c:0336` |
| 进入方式 | Sony-PMCA-RE `senserShellCommand()`（libusb） |
| 认证结果 | `readHasp()` 返回 88 字节，认证成功 |

> 注意：A7M3 的海量存储 PID 是 `0c02`，与 A7C II 的 `0e8a` 不同。PYUSB 需要 root
> 或 udev 规则；本次以 `wsl -u root` 运行。

### 11.2 服务模式 Jiritsu 请求布局（实测）

对命令 `0x11`（get_file2）做单字节差分探测后，可确定请求体布局：

```text
01 10 <cmd> <b3> <type> <index> <b6>
 |  |   |     |     |      |      +-- wide/flag（本次未观察到独立效果）
 |  |   |     |     |      +--------- 索引（0..15）
 |  |   |     |     +---------------- 类型（0x00 / 0x01 / 0xff）
 |  |   |     +---------------------- 其他字段（置 0xff 触发 Senser 层错误 128）
 |  |   +---------------------------- 命令号
 |  +-------------------------------- 固定 0x10
 +----------------------------------- 固定 0x01
```

### 11.3 实测矩阵（probe5，全部只读）

| 请求体 | 返回状态（小端） | 解释 |
|---|---|---|
| `01 10 0d 00` | `0x10000006` | version：`/tmp/jiritsu/ver.txt` 不存在 |
| `01 10 0f 00 00` + `/tmp/jiritsu/ver.txt` | `0x00000000` | set_file_path 成功（仅保存路径） |
| `01 10 11 00 00 ff 00` | `0x10000004` | type=0、index=0xff → 尝试挂载 `/dev/nflasha6` 失败 |
| `01 10 11 00 00 01 00` | `0x10000004` | type=0、index=1 → 同样挂载失败 |
| `01 10 11 00 00 00 00` | `0x10000201` | index=0 非法 |
| `01 10 11 00 00 02 00` | `0x10000201` | index=2 非法 |
| `01 10 11 00 01 ff 00` | `0x10000001` | type=1 → `sys_media_mount` 失败（可能无 SD 卡） |
| `01 10 11 00 02 00 00` | `0x10000201` | type=2 非法 |
| `01 10 08 00 00 02 00` | `0x10000004` | get_file 默认参数失败 |
| `01 10 08 00 01 00 00` | `0x1000000a` | get_file type=1 → `/mnt` 路径不可用 |

关键判据：同一请求只改 `body[4]`/`body[5]` 即出现不同错误码，
说明这两个字节分别对应 `type` 与 `index`，与静态分析一致。

### 11.4 静态分析（`nflasha15_unpacked/lib/libJiritsu.so`，4.04）

使用 capstone（Thumb 模式）反汇编，并按 PLT/GOT 重定位解析导入函数：

- `adj_cmd_get_version`：读取 `/tmp/jiritsu/ver.txt`；不存在时返回 `0x10000006`。
- `adj_cmd_get_file2`（cmd `0x11`）：
  - `type=0x00`：`sys_mount("/dev/nflasha6", "/tmp/jiritsu")`；成功后按
    `get_access_filename(index)` 取固定文件名，`stat` + `open` + `read`。
    index 只允许 `1,3..15`；`0`/`2` 非法。
  - `type=0x01`：调用 `sys_media_mount(...)`，实测返回 `0x10000001`。
  - `type=0xff`：按 index（0..15）从**内存中的调整数据表**拷贝，不接触文件系统。
  - 其他 type：`0x10000201`（请求非法）。
- `adj_cmd_get_file`（cmd `0x08`）：
  - `type=0x00`：挂载 `/dev/nflasha6` 后按索引读固定“区域文件”（tmmc.jit、libJit.so、
    jit.log、history.txt、DIAC.csv 等）。
  - `type=0x01`：路径 = `"/mnt"` + 请求负载 → **只能读 SD 卡**。
  - `type=0x02`：固定路径。
- `adj_cmd_set_file_path`（cmd `0x0f`）：把路径保存到全局变量（上限 `0x1fe`），
  将 `\` 转成 `/`，遇到 LF/CR/逗号则丢弃；**不做前缀/目录限制**，但该全局路径
  仅被 `get_file2` 内部使用，未发现可越权到任意根路径的分支。

### 11.5 结论

1. **服务模式认证与 Jiritsu 控制命令可用**（version / set_file_path / 部分读取）。
2. A7M3 4.04 的 `libJiritsu.so` **没有** A7C II 那样“`/tmp/jiritsu/../..` 遍历任意
   路径”的读取语义。其可读对象被限制为：
   - `/tmp/jiritsu`（需 `/dev/nflasha6` 成功挂载）下的固定命名文件；
   - `/mnt`（SD 卡）；
   - 内存中的调整数据表（`type=0xff`）。
3. 因此，**在 A7M3 上仅靠服务模式 + Jiritsu 无法读取 `/proc`、`/etc`、`/setting`
   或 boot 分区**。A7C II 的 preload 读取链不能直接照搬到 A7M3。
4. 当前服务模式下 Jiritsu 区未初始化：version 报文件不存在，`/dev/nflasha6` 挂载失败，
   媒体挂载失败（可能未插 SD 卡）。

### 11.6 复现与后续

- 探测脚本已保存到 `$HOME/firmware/a7m3/service-probe/`：
  - `a7m3-jiritsu-probe5.py`：请求布局差分探测（本次主要证据）
  - `a7m3-jiritsu-probe6.py`：`type=0xff` 内存表探测（**尚未执行**，见下）
  - `a7m3-libjiritsu-disasm2.txt`：带 PLT 符号的 `libJiritsu.so` 反汇编
- 继续探测需要人工操作：
  1. **断电重启相机**（本次服务会话结束后设备已不再枚举，符合 `054c:096f` 残留状态）；
  2. 插入 SD 卡（用于验证 `type=1` 的 `/mnt` 分支）；
  3. 重新以 `wsl -u root` 运行 `a7m3-jiritsu-probe6.py`。
- 若要读取根文件系统/ boot 分区，A7M3 仍应走：
  - 持久 root 路线：`/setting/mode/dmode` + `/setting/env.txt`（init 既有逻辑）；
  - 或硬件 eMMC 路线：UART + fastboot / boot0 读取（参考 A7M4 的 emmc_boot 思路）。

### 11.7 设备状态

- 本次未写入相机任何文件。命令 `0x11 type=0` 会让相机自身尝试挂载
  `/dev/nflasha6`，实测返回失败（`0x10000004`），未产生写入。
- 探测结束后相机离开服务模式，Windows/USBIP 已看不到设备；恢复到普通海量存储
  需要**断电重启**。

---

## 12. 服务模式完整固件转储（2026-10-31 续，成功）

第 11 节只测试了 Jiritsu（AdjustControl `0x40`）的读取接口，结论是它受限于调整区/SD 卡。
本次改用 **FileControl（Senser pFunc `0xff01`，cmd=2 READ）**，可以直接读取相机根文件系统
和**块设备节点**，从而完整转储了 eMMC 分区。

### 12.1 方法

1. PMCA `senserShellCommand()` 从海量存储 `054c:0c02` 切入服务模式 `054c:0336`；
2. `readHasp()` 认证（88 字节）；
3. 对每个 `/dev/nflashaN` 调用 `SonySenserCamera.readFile(path, fileobj)`；
4. 每个分区保存为独立 `.bin`，记录大小、耗时和 SHA-256；
5. 会话结束由 `dev.stop()` 自动把相机切回 `054c:0c02`（本次无需断电）。

全部操作为**只读**：没有写文件、没有删除、没有格式化、没有刷写。

### 12.2 相机固件版本

- `/setting/updater/dat4` 读出 2 字节 `03 04`；
- PMCA 的 `FirmwareVersionProperty` 解析为 **4.03**；
- 因此相机当前运行 **4.03**，而手头的官方更新包是 **4.04**——两者分区镜像不同，
  这是版本差异，不是读取错误。

### 12.3 转储结果

输出目录：`$HOME/firmware/a7m3/service-dump/`（约 1.27 GiB）
清单：`service-dump/MANIFEST.md`（含每个分区的 SHA-256 与文件系统识别）。

| 分区 | 大小 (B) | 识别 |
|---|---:|---|
| nflasha1 | 8,323,072 | FAT12（8 MiB） |
| nflasha2 | 20,971,520 | FAT12（20 MiB） |
| nflasha3 | 50,331,648 | FAT，文件系统 31,493,120 B（kernel + initrd） |
| nflasha4 | 12,582,912 | Sony 容器，magic `CMMeX` |
| nflasha5 | 62,914,560 | Sony 容器，magic `WBI1` |
| nflasha6 | 16,777,216 | FAT12（16 MiB） |
| nflasha7 | 8,388,608 | ext2（5,000 块）；根骨架；`/version.txt` = `37114_12.2025061901` |
| nflasha10 | 12,582,912 | FAT12（12 MiB） |
| nflasha11 | 524,288,000 | FAT16（500 MiB） |
| nflasha12 | 41,943,040 | FAT12（40 MiB） |
| nflasha13 | 4,194,304 | FAT12（4 MiB） |
| nflasha15 | 314,572,800 | ext2（211,496 块）；`/usr` 树 |
| nflasha16 | 146,800,640 | 整个文件全零（后续验证） |
| nflasha17 | 104,857,600 | FAT16（100 MiB） |
| nflasha18 | 20,971,520 | FAT12（20 MiB） |
| nflashaB0 | 4,194,304 | **EXBL 引导镜像**，magic `EXBL` |
| nflashaB1 | 4,194,304 | 全零 |

不存在的节点：`nflasha0/8/9/14`、`nflashaB2`（FileControl 返回 Senser 130）。

### 12.4 完整性判据

- 每个 FAT 分区的 `sectors` 字段 × 512 与读回文件大小逐一相等
  （如 nflasha11：1,024,000 × 512 = 524,288,000 = 文件大小）；
- ext2 分区 `dumpe2fs` 的 block count × 1024 均在分区大小以内，且文件系统 UUID 可解析；
- `/dev/nflashaB0` 以 `EXBL` 开头，非零数据止于 0x272DA 左右，其余为 0，符合 EXBL 镜像特征；
- `B1` 全 4 MiB 为 0，与 A7M4 的 boot1 一致；
- 交叉验证：
  - 相机 `/sbin/init` 与 4.04 包 initrd 的 `/sbin/init` SHA-256 完全一致；
  - 相机 `/usr/bin/launch_shell.elf` 与 4.04 包 `bin/launch_shell.elf` 一致；
  - 相机 `/usr/lib/libJiritsu.so` 与 4.04 包不同（4.03 vs 4.04，预期）。

### 12.5 与第 11 节结论的关系

- 第 11 节的结论仍然成立：**Jiritsu（pFunc `0x40`）不能读任意文件**；
- 但 **FileControl（pFunc `0xff01`）可以**，包括 `/dev/nflasha*` 块设备；
- 因此“服务模式不能保存固件”的说法被修正为：**Jiritsu 不行，FileControl 可以**。

### 12.6 后续

- `nflashaB0`（EXBL）已取得，A7M3 的 QEMU EXBL 启动尝试不再缺 boot0；
- 下一步可用 `nflashaB0` + `nflasha3`（kernel/initrd）重试 ma1co/qemu 的 cxd90045 机器；
- 完整 4.03 分区镜像可用于与 4.04 更新包做差分、分析升级改动了哪些组件。

### 12.7 复现脚本

`$HOME/firmware/a7m3/service-probe/`：
- `a7m3-dump.py`：分区转储（FileControl READ，只读）
- `a7m3-run-dump.sh`：以 root 运行并 tee 日志
- `a7m3-fwprobe2.py`：路径矩阵探测（发现 `/dev/nflasha*` 可读）
- `a7m3-manifest.py`：生成 MANIFEST.md

---

## 13. QEMU 启动 A7M3：EXBL 与 -kernel 两条路线（2026-10-31 续）

在拿到完整 4.03 分区转储（含 `nflashaB0` EXBL）后，用 ma1co/qemu 的 `cxd90045`
机器继续尝试启动。

### 13.1 EXBL 路线

- 构造镜像：`[boot0 前 256 KiB][boot1 256 KiB][256 MiB 用户区（全零）]`；
  用户区大小必须是 2 的幂（SD 模型要求 `image_size - 2*boot_size` 为 2 的幂）。
- QEMU 用 `-drive if=mtd,format=raw,file=qemu-emmc.img`。
- 结果：**EXBL 确实执行**。`-d in_asm` 显示 CPU 进入 `0xfe047000`（boot0 的
  `loader_base`），执行 DRAM 初始化代码，随后在 `0xfe04bee0..0xfe04bf2a`
  进入一个“延时 + GPIO 翻转”的循环，反复调用 `0xfe04d8e6`（hwtimer 延时）。
- 该循环附近的字符串有 `ERROR:Bkup`、`Del LOG`、`LDR:Reset Bkup`，且循环会
  一直翻转三个 GPIO（LED 闪烁），符合“找不到下一阶段/备份区、进入错误指示”的行为。
- 用户区目前全零，没有 SDM2 分区表和 kernel 分区，因此 loader 无法继续。
- 结论：EXBL 路线可行的关键是把**合法的 eMMC 用户区**（分区表 + 内核分区）
  构造出来；或者进一步逆向 loader 读取用户区的偏移/校验。

### 13.2 `-kernel` 路线

关键事实（从内核镜像本身提取）：

| 项 | 值 |
|---|---|
| 内核链接基址 | `_stext = 0x60108160`（kallsyms），镜像加载到 `0x60108000` |
| `KERNEL_RAM_PADDR` | `0x60000000`（head.S 字面量） |
| machine_desc | `nr = 0x11a`，name = `ARM-CXD900X0` |
| 内核物理基址 | `0x60000000`（恒等映射） |

> 注：上表的 `DDR0_BASE=0x60000000` 是中间实验；最终 QEMU 模型使用
> `DDR0_BASE=0x00000000` + `PAGE_OFFSET=0x60000000`。

本地 QEMU 机器做了以下修正（仅本地副本）：

- `CXD90045_DDR0_BASE = 0x60000000`，1 GiB；移除重叠的 DDR1；
- 高向量页 `0xffff0000` 改为可写 RAM（内核会把异常向量复制到这里）；
- ATAGS 字节序修正：**先 size 后 tag**（内核 `validate_atags` 的判据是
  `[r2] == 5/2`、`[r2+4] == 0x54410001`）；
- 机器 ID `0x11a`，ATAGS 放在 `0x60010000`，initrd 放在 `0x60700000`。

结果：内核能执行到 `paging_init`，随后在 `0x6049cb98` 写页表时触发
Data Abort：

```text
PC 0x6049cb98 DFAR 0x60105c00
DFSR 0x805 (write, section translation fault)
TTBR0 0x6010406a  TTBCR 0x0  SCTLR 0x10c5387d
```

### 13.3 根因

- 内核的 `__create_page_tables` 只映射了 `__enable_mmu` 所在的那一个 1 MiB
  段（字面量 `0x603c8894`/`0x603c88d8`）；
- **内核镜像本身的页表映射由 Sony 的引导程序（EXBL）预先建立**；
- 使用 `-kernel` 时没有 EXBL 的页表，`swapper_pg_dir`（`0x60104000`）里没有
  映射 `0x60100000..0x601fffff` 的段项；`paging_init` 一旦访问页表页自身
  （`0x60105c00`）就发生翻译故障，异常向量又落在未映射的高向量页，形成
  无限 Prefetch Abort 循环。

尝试把 `__create_page_tables` 的映射范围字面量改成 `0x60000000..0x60600000`
（`vmlinux-patched.bin`）**没有效果**，说明镜像映射不是由这个循环完成的，
进一步佐证“映射来自引导程序”。

### 13.4 结论与下一步

- `-kernel` 捷径对该内核**根本不够**：必须先由引导程序建立页表；
- 继续的正确路线是 **EXBL**：
  1. 构造 eMMC 用户区（SDM2 分区表 + kernel 分区 + 必要备份结构），
     或用 `nflashaB0` 逆向 loader 期望的偏移与校验；
  2. 让 EXBL 完成 DRAM/页表/ATAGS 设置后跳入内核；
- 已保存的产物：`$HOME/firmware/a7m3/qemu-emmc.img`、
  `qemu-exbl.log`、`qemu-int*.log`、`vmlinux-patched.bin`；
- QEMU 本地改动仅存在于 `$HOME/qemu-ma1co`（`hw/arm/bionz.c`、
  `target/arm/helper.c` 的调试输出），未提交。

---

## 14. QEMU 完整启动到相机 BusyBox（2026-10-31 续，成功）

> 本节取代第 13.4 节的结论：不需要 EXBL，也不需要从引导程序取得页表。
> 页表、ATAGS、initrd 全部由 QEMU 机器代码在装入内核前预置，内核只需一个
> 极小的 head 补丁恢复被 Sony 早期初始化清掉的 ATAGS 指针。

### 14.1 启动链

1. **QEMU 侧预置页表**（`hw/arm/bionz.c`）：在物理 `0x00104000` 写入 1 个
   L1 段描述符 `pgd[0x600] = PA(0x00000000) | 0x00011c0e`，先把内核链接地址 `0x60000000` 起的 1 MiB 映射到物理 0，其余由内核 head 建立。内核 head.S 会先
   identity map，再跳到链接地址 `0x6049605c`；预置项保证这条路径可走。
2. **正确 ATAGS**：`ATAG_MEM` 负载按内核 `struct tag_mem32` 的字段顺序
   **先 size 后 start**；`ATAG_INITRD2` 用 `0x54420005`（不是已废弃的
   `ATAG_INITRD 0x54410005`）。initrd 放在 PA `0x00700000`（原稿误写为 0x01000000）。
3. **kernel head 补丁**（`vmlinux-atagfix.bin`）：Sony 的 `stext` 会调用
   `0x6011efa0` 并破坏 `r2`（ATAGS 指针），导致 `__vet_atags` 把指针清零。
   在 `0x60108028` 插入 `bl`，跳到 `0x60495f40` 的 stub：
   `ldr r2, [pc, #4]; b __vet_atags`，字面量 `0x00010000`。
4. **eMMC 用户区**：由服务模式转储的 `nflashaN` 镜像拼成 2 GiB 稀疏镜像，
   MBR 主分区 1–3 + 扩展分区（逻辑分区 5–25），分区大小取自
   `service-dump/MANIFEST.md`；未知分区 8/9/14/19–25 用零占位。
   分区 2/3/7/10/11/12/15/18 等写入真实转储内容。
5. **eMMC 模型**：QEMU 的 `sd.c` 把用户区放在镜像偏移 `2*boot_size`
   = `0x80000`；所以镜像 = 0x80000 字节前缀 + 2 GiB 用户区。
6. **Sony 分区表**：eMMC 驱动自己的 `ptbl`（ALFReD “Information table”）
   仍未重建，日志里有 `partition table invalid`；这不影响内核的标准 MBR
   扫描，`/dev/nflasha1..25` 全部出现。`/dev/dnflashaN`（压缩分区）因此
   不存在，init 会自动回落到 `nflashaN`。

### 14.2 两个让用户态真正跑起来的补丁

- **grm_gles.ko 去初始化**（在 eMMC 镜像内原地改 8 字节）：
  `init_module`（模块文件偏移 `0x1944`，`.init.text+0x144`）改为
  `mov r0,#0; bx lr`。否则 `sugilite_clock_up_internal` 在 QEMU 里触发
  `kernel BUG at sugi_hw.c:221`，init 作为 pid 1 被杀。
- **串口控制台使能绕过**（`vmlinux-console.bin`）：Sony 把 PL011 控制台输出
  用 ADC/errind 机制门控（`em_amba2_console_enable` →
  `errind_console_enable_triggered` 读取 ADC 值），QEMU 没有该硬件，
  门控字段永远为 0，`pl011_console_write` 直接返回。
  把 `pl011_console_setup+0x74`（VA `0x602988cc`）的
  `str r3,[r6,#0xd4]`（`0xE58630D4`）改为
  `str r2,[r6,#0xd4]`（`0xE58620D4`，`r2` 此时已为 1），
  控制台即可输出。补丁后镜像 SHA-256：
  `e6c17a94b813111d3a70b95f303d60c07b40da399146a4c0038d075976169b0f`。

### 14.3 启动命令与进入 shell

```bash
qemu-system-arm -M cxd90045 \
  -kernel $HOME/firmware/a7m3/vmlinux-console.bin \
  -initrd $HOME/firmware/a7m3/initrd-4.03.img \
  -drive if=mtd,file=$HOME/firmware/a7m3/qemu-emmc-2g.img,format=raw \
  -serial stdio -display none
```

启动过程中：

- `im.elf`（pid 99）会在起完模块后因 Sony FixedArea `mmap` 失败
  （`FixedAreaTop: FFFFFFFF`）触发 Data Abort，内核进入 Sony 异常监视器，
  串口出现 `Exception>` 提示符；
- 在 `Exception>` 输入 **`exit`**（输出 `Good Bye.`），系统继续运行，
  随后就是相机 rootfs 的 BusyBox ash：`/ #`。

也可以用异常监视器自己的 `shell` 命令直接进入 `/bin/sh`。

### 14.4 实测证据（2026-10-31）

```text
/ # uname -a
Linux (none) 3.0.27_nl-rt106+ #1 SMP PREEMPT RT Thu Jun 19 18:15:47 JST 2025 armv7l GNU/Linux

/ # cat /version.txt
37114_12.2025061901

/ # cat /proc/partitions
 252        0    2097152 nflasha
 252        1       8128 nflasha1
 252        2      20480 nflasha2
 252        3      49152 nflasha3
 252        4          1 nflasha4        (扩展分区容器)
 252        5      61440 nflasha5
 ...
 252       25       4096 nflasha25
 252      126        256 nflashaB0
 252      127        256 nflashaB1

/ # cat /proc/mounts
/dev/nflasha7  /        ext2 ro,...
/dev/nflasha15 /usr     ext2 ro,...
/dev/nflasha10 /tmp     vfat rw,...
/dev/nflasha10 /etc     vfat rw,...
/dev/nflasha3  /system  vfat rw,...
/dev/nflasha2  /setting vfat rw,...
/dev/nflasha11 /log     vfat rw,...
/dev/nflasha18 /lens    vfat rw,...
/dev/nflasha12 /cert    vfat rw,...
```

完整的启动串口日志保存在 `$HOME/firmware/a7m3/qemu-console.log`
（脚本见 `$HOME/firmware/a7m3/qemu/`）。

### 14.5 尚未解决的问题

| 现象 | 原因 | 影响 |
|---|---|---|
| `im.elf` 崩溃：`page translation fault at 0x0`，`FixedAreaTop/Size = FFFFFFFF`、`Failed to mmap` | Sony FixedArea（固定物理区/寄存器窗口）在 QEMU 不存在；后续定位为缺少 `/dev/dmpgles2`（248:29） | 相机 UI 无法启动；异常监视器可用 `exit` 退出 |
| `backup.ko` `backup_init() error (file_load)` | backup 数据/分区（可能依赖 ptbl/WBI）缺失；后续用 `Backup_load=1` 解决 | `upm/mcmn_drv/ms_drv/mmc_drv` 等因缺符号加载失败 |
| `/proc/udm/upm_bootmode` 打不开 | `upm.ko` 未加载；后续用 `memrsv` 解决 | init 走 `launch_shell` 分支而不是正常 `bootin.elf` 分支 |
| `dma330.ko` 初始化约 10 s 后返回 `-14`（`alloc_inst_area` 失败） | DMA330 硬件未建模 | 只是延迟启动，无致命影响 |
| Sony `ptbl` / `dnflasha` / WBI 不存在 | 服务模式转储只覆盖 `nflashaN`，未取得 Information table | 压缩分区、部分备份功能不可用 |

### 14.6 产物与复现脚本

- `$HOME/firmware/a7m3/vmlinux-atagfix.bin` — ATAGS stub 补丁内核
- `$HOME/firmware/a7m3/vmlinux-console.bin` — 串口使能补丁内核
- `$HOME/firmware/a7m3/qemu-emmc-2g.img` — 合成的 2 GiB eMMC 镜像
  （MBR 25 个分区 + 转储内容 + grm_gles 补丁）
- `$HOME/firmware/a7m3/qemu/build-emmc.sh` — 生成 eMMC 镜像
- `$HOME/firmware/a7m3/qemu/patch-kernel.py` — 生成/校验内核补丁镜像
- `$HOME/firmware/a7m3/qemu/patch-grmgles.py` — 在 eMMC 镜像内跳过
  grm_gles 初始化
- `$HOME/firmware/a7m3/qemu/run-shell.sh` — 启动 QEMU 串口控制台

QEMU 侧改动仍只存在于 `$HOME/qemu-ma1co`，未提交：

- `hw/arm/bionz.c`：DDR 基址/大小、CMDLINE（含 `log_buf_len=4M`）、
  pgd 预置、ATAGS 修正；
- `target/arm/helper.c`：Data Abort 调试输出；
- `hw/char/pl011.c`：控制台读写跟踪（可移除）。

> 当前发布包的脚本名与流程以公开仓库的 `../BUILD.md` 为准
> （如 `patch-console.py`、`patch-atagfix.py`、`build-emmc.sh`）。

---

## 15. 整理进固件包时的新增发现

- `nflasha1/BOOT` 已确认是 **updater/recovery 环境**：zImage + cramfs
  （版本 `100.101.005`，562 文件），`/sbin/init` 是升级脚本；`KEMCO.TXT`
  保存 updater 内核的引导参数（`mem=1G@0`、`mem=1016M@0x80000000@1`、
  `memrsv=0x3600000@0x20000000:uc`）。
- **主内核参数在 `nflasha2:/kemco.txt`**（1,901 B），不是上面的 updater
  文件：`mem=1G@0`、`mem=256M@0x80000000@1`、
  `memrsv=704M@0x14000000:uc`、`14M@0x80000000:uc`、
  `242M@0x80e00000:uc`。详见 `FINDINGS.md` §3.0。
- `nflasha16` 整个 146,800,640 B 全零；`nflasha4`/`nflasha5` 分别是
  CMMeX/WBI1 容器，尚未展开。
- QEMU 侧已把 `grm_gles` 从“整段打桩”推进到真正注册字符设备
  **`dmpgles2`（248:29）**；补上 `/dev/dmpgles2` 后 Appframework 能
  打开并 mmap 128 KiB，`FixedAreaTop/Size` 变为 `20098000/0007C000`。
- 后续确认 `libmpr` 崩溃的根因是 **DFB 在 1 扇区 `nflasha4` 上写失败后的
  二次加锁**；改到 `/dev/nflasha16` 后 `im.elf` 可运行到 63 线程。当前主
  阻塞是 LIRO/CP 的 `0xf2a1xxxx` 硬件忙等；详见
  `../docs/LIRO-AND-MEMORY.md`。
- 当前发布包的分析文档在 `firmware/FINDINGS.md`、`MANIFEST.md`、
  `PARTITIONS.tsv`；完整二进制在仓库外的本机包。
