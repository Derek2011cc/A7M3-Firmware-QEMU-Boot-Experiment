# 内存模型、DFB 崩溃与 LIRO 固件执行（2026-10-31 晚更新）

本文记录最近一轮实验的结论，修正了之前两处重要判断：

1. **主内核的真实启动参数不在 `nflasha1/BOOT/KEMCO.TXT`**，而在
   `nflasha2:/kemco.txt`；DDR1 是 **256 MiB**，不是 updater 参数里的 1016 MiB。
2. Appframework 早先的 `libmpr` NULL `MemInfo`/互斥量崩溃，根因是
   **DFB 在 1 扇区的 `nflasha4` 上写失败**，直接触发点是错误路径二次加锁；
   把 DFB 目标改到 `/dev/nflasha16` 后，`im.elf` 可以稳定运行到 **63 线程**。

同时，LIRO 协处理器固件已经能在 QEMU 中执行 `entry_pre`/`entry_main`，
但随后会在一个未建模的 `0xf2a1xxxx` 硬件块上忙等，抢死 Linux。这是目前
原生 UI 之前的主要阻塞。

> 所有二进制、分区转储、内核/initrd、eMMC 镜像和运行日志都不随仓库分发。
> 本文只记录地址、字节、哈希和判据。

---

## 1. 主内核的真实启动参数：`nflasha2:/kemco.txt`

### 1.1 定位

`nflasha2`（FAT12，挂载为 `/setting`）里有一个 **1,901 字节**的
`kemco.txt`：

| 项目 | 值 |
|---|---|
| 路径 | `nflasha2:/kemco.txt` |
| 大小 | 1,901 字节 |
| SHA-256 | `83b32a57278ee640e27fb60cbeec28a9c74b6055f5fe1b2b728d1c053b5988c8` |
| 在合成 eMMC 镜像中的文本位置 | 文件偏移 `0x9bcad0`（`nflasha2` 分区内偏移 `0x3cad0`） |

`nflasha1/BOOT/KEMCO.TXT`（815 字节）属于 **updater/recovery 启动对**，
不是主内核参数。两份文件内容不同，之前“主内核可能复用 updater 参数”的
判断已被否定。

> 证据边界：尚未在串口日志中直接抓到 bootloader/init 读取
> `nflasha2:/kemco.txt` 的调用；把它作为主内核参数使用的依据是：
> 文件位于 `/setting`、以 `kemco` 开头、内容是完整启动参数，且按其中的
> 内存行在 QEMU 中启动后，LIRO 的 `arch_phys_to_noncache` 行为与预期一致
> （见 §2）。

### 1.2 内存相关原文

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

由此得到主内核的内存地图：

| 区域 | 物理地址 | 大小 | 属性/用途 |
|---|---|---:|---|
| DDR0 | `0x00000000` | 1 GiB | `mem=1G@0` |
| 低端保留 | `0x00000000` | 1 MiB | `memrsv=1M@0`（默认类型） |
| 低端保留 | `0x00F00000` | 32 KiB | `memrsv=0x8000@0xF00000` |
| 低端保留 | `0x02E68000` | `0x1A5E000` | klog/alog/boottime/固件等 |
| 低端保留 | `0x09915000` | `0xA6EB000` | 大块保留 |
| 低端 `uc` | `0x14000000` | 704 MiB | `memrsv=704M@0x14000000:uc` |
| DDR1 | `0x80000000` | **256 MiB** | `mem=256M@0x80000000@1` |
| DDR1 `uc` | `0x80000000` | 14 MiB | `memrsv=14M@0x80000000:uc` |
| DDR1 `uc` | `0x80E00000` | 242 MiB | `memrsv=242M@0x80e00000:uc` |

要点：

- **DDR1 只有 256 MiB**；`memrsv=14M + 242M` 恰好覆盖整个 DDR1，且全部为 `uc`。
- 低端 `0x14000000..0x3FFFFFFF` 整段是 704 MiB `uc`，不是 updater 的
  54 MiB `0x20000000` 窗口。
- LIRO 的缓冲区地址 `0x8bdc1200..0x8bdc13dc` 落在
  `0x80E00000..0x8FFFFFFF` 的 242 MiB `uc` 区间内。

主内核 `kemco.txt` 的其余参数（`console=ttynull`、`wbi_memclr`、
`em_console`、`wbimem=...`、`pm.*` 等）也在同一文件中；本实验的 QEMU
命令行只取内存相关行，并用 `console=ttyAM0,115200` 覆盖 `ttynull` 以便
采集串口日志。

---

## 2. Sony 非缓存地址转换与 `memrsv` 表

### 2.1 函数与表

| 符号/地址 | 作用 |
|---|---|
| `parse_memrsv` `0x6049ad34` | 解析 `<size>@<addr>[:<type>]` |
| 区间登记函数 `0x6049d55c` | 把 `memrsv` 区间写入非缓存地址表；只接受掩码 `0x14819` 中的类型 |
| `arch_phys_to_noncache` `0x6011f2d0` | PA → 非缓存 VA |
| 非缓存地址表 `0x604e07e0` | 5 个 32 位值，运行时由 `memrsv` 填充 |

表的静态初值：

```
0x604e07e0: 40000000 90000000 a0000000 b0000000 ffffffff
```

`parse_memrsv` 对每个匹配类型的区间做“取最小起始地址”的更新：

- 低端区间（`< 0x40000000`）更新 `table[0]`，别名写 `table[2]`；
- `0x80000000..0x8FFFFFFE` 区间更新 `table[1]`，别名写 `table[3]`。

关键点：**`mem=`/ATAG 提供的内存 bank 不会进入这张表，只有 `memrsv`
会。** 因此仅恢复 DDR1 物理内存而不加 `:uc` 的 `memrsv`，LIRO 的
`0x8bdc1200` 仍会被拒绝。

### 2.2 使用真实参数后的运行值

在 QEMU 中按主内核参数启动后，从客户机 `/dev/mem` 读
PA `0x004e07e0`（内核 VA `0x604e07e0`）：

```
00000014 00000080 00000074 000000a0 ffffffff
```

即：

```
table[0] = 0x14000000   /* 704M uc 的起点 */
table[1] = 0x80000000   /* DDR1 的起点 */
table[2] = 0x74000000   /* 0x14000000 + 0x60000000 */
table[3] = 0xA0000000   /* 0x80000000 + 0x20000000 */
table[4] = 0xFFFFFFFF
```

于是 `arch_phys_to_noncache(0x8bdc1200)` 走 `table[1]` 分支，返回
`0xabdc1200`。之前 129 条
`ERROR:arch_phys_to_noncache:pa=0x8bdc12xx` 全部消失。

### 2.3 失败/被否定的方案

| 方案 | 结果 |
|---|---|
| 1 GiB ATAG + `memrsv=0x20000000@0x20000000:device` | 能启动，但 `0x8bdc1200` 被拒绝（旧简化模型） |
| DDR1 1016 MiB @ `0x80000000` + 旧 cmdline | LIRO 仍报同样的 `0x8bdc1200` 错误；1016 MiB 是 updater 参数，不是主内核 |
| DDR1 256 MiB + 主内核 `memrsv`（本文方案） | 报错消失，`entry_main` 正常返回 |

---

## 3. QEMU 模型改动

`hw/arm/bionz.c` 当前配置：

- DDR0：`0x00000000` + 1 GiB；
- DDR1：`0x80000000` + **256 MiB**（`0x10000000`）；
- ATAGS：两个 `ATAG_MEM`（1 GiB @ 0、256 MiB @ `0x80000000`）+ cmdline；
- cmdline（内存部分）：

  ```
  memrsv=1M@0
  memrsv=0x8000@0xF00000
  memrsv=0x1A5E000@0x02E68000
  memrsv=0xA6EB000@0x09915000
  memrsv=704M@0x14000000:uc
  memrsv=14M@0x80000000:uc
  memrsv=242M@0x80e00000:uc
  ```

  `mem=1G@0`、`mem=256M@0x80000000@1` 通过 ATAG_MEM 提供，不重复写进
  cmdline。

启动日志判据（来自本地串口日志，不随仓库分发）：

```
arm_add_memory: 0 0x0 0x40000000
arm_add_memory: 1 0x80000000 0x10000000
high_memory: b0000000
Memory: 1024MB 256MB = 1280MB total
Memory: 104856k/104856k available, 1205864k reserved, 0K highmem
```

可用内存只有约 102 MiB，是因为 704 MiB + 256 MiB 的 `uc` 区间被
`memrsv` 保留；这与主内核参数一致（保留区由用户态/协处理器直接管理）。
在这个模型下、不预加载 LIRO 固件时，`im.elf` 仍能运行到 63 线程。

---

## 4. DFB / `nflasha4` 崩溃根因与绕过

### 4.1 根因

`libmpr.so`（4.03 运行时，SHA-256
`ab1df757108d123f403188e33b732bf0c8cded33ff29711417dca14c5ab883a5`）
的 DFB 初始化路径：

- `DFB::Init` `0x33ebf0` → `DvdFlashBackupLib::Init` `0x346258`；
- `DFA::OpenDriver` `0x338ce0` 根据 EEP 标志打开
  `/dev/nflasha4`（或 `/dev/cnflasha4`）；
- 本实验的合成 MBR 把 `nflasha4` 做成 **1 个扇区**的扩展容器，
  `DFA::FlashWrite` `0x339a38` 的 `lseek` 越过 512 字节即失败；
- 错误路径在已经持有 MemInfo 互斥量的情况下，再次调用带加锁的
  scoped helper `0x70354c`（正常互斥量，owner 非 0），触发 glibc
  2.11.2 的 `pthread_mutex_lock.c:62` 断言，进程 SIGABRT。

DFB 失败是上游原因；早先观察到的“`__pthread_mutex_lock(NULL+0x1c)` /
`MemInfo` 为空”是其下游表现之一，而错误路径的二次加锁断言是直接触发点。
根因在 DFB 的 1 扇区 `nflasha4`。

### 4.2 绕过

在 `libmpr.so` 副本中把 DFB 的打开路径改为 `/dev/nflasha16`
（合成镜像中的 140 MiB 全零分区）：

| 文件偏移 | 原字节 | 新字节 | 说明 |
|---|---|---|---|
| `0xbe54e0` | 15 个 `00` | `/dev/nflasha16\0` | 字符串新位置（原有零填充区） |
| `0x338e14` | `98 54 be 00` | `e0 54 be 00` | 原指向 `0xbe5498` 的 literal → `0xbe54e0` |
| `0x33a858` | `98 54 be 00` | `e0 54 be 00` | 同上（第二处打开路径） |

脚本：`scripts/experimental/patch-libmpr-dfb16.py`（只改宿主机上的
`libmpr.so` 副本，不接触真机；补丁库需自行放回 eMMC 镜像）。

结果：

- 不再出现 `pthread_mutex_lock` 断言和 trim 错误；
- `im.elf` 稳定运行，`Threads: 63`；
- `libmpr.so`、`libmprctrl.so`、`libmprdummy.so` 均已映射。

这是**实验性绕过**，不是对 DFB/FlashBackup 的真实硬件模拟。若以后重建
`nflasha4` 的 CMMeX 内容或 WBI 路径，应撤销该补丁。

放回 eMMC（在你自己的 `nflasha15.bin` 副本上操作）：

```sh
debugfs -R 'dump /lib/libmpr.so libmpr.so' nflasha15.bin
python3 scripts/experimental/patch-libmpr-dfb16.py libmpr.so libmpr.dfb16.so
debugfs -w -R 'rm /lib/libmpr.so' nflasha15.bin
debugfs -w -R 'write libmpr.dfb16.so /lib/libmpr.so' nflasha15.bin
# 然后用修改后的 nflasha15.bin 重新运行 scripts/build-emmc.sh
```

`write` 后的文件块不保证与原文件连续，但 ext2 会正常记录新的块链；
`build-emmc.sh` 只按分区内容写入，不依赖固定文件偏移。

---

## 5. LIRO 固件执行与 `0xf2a1` 忙等

### 5.1 固件与加载

| 项目 | 值 |
|---|---|
| 文件 | `nflasha3:/AV-CAM.BIN`（4.03） |
| 大小 | 18,449,784 字节 |
| SHA-256 | `e1b0f84e8323005a34b43de6dd0083fca712d2c654f0d9716512e0a54631bbe7` |
| 头部 | `0a0000ea 4f52494c 04000000 ...`（magic `ORIL`，version 4） |
| 加载 PA/VA | PA `0x035c6000` / VA `0x635c6000` |
| 入口 | `entry_pre=0x635c6044`，`entry_main=0x635c6058` |

QEMU 侧用 `-device loader,file=...,addr=0x035c6000,force-raw=on`
预加载（本实验做法）；`liro.ko` 读取 magic/version/入口后执行。

### 5.2 执行与线程

本地日志中：

```
start entry_pre(635c6044)
end entry_pre
start entry_main(635c6058)
end entry_main
```

在 LIRO 初始化过程中，任务列表出现 **至少 134 个** `liro-kliro_<N>`
内核线程（`entry_main` 返回后遍历任务链，pid 到 284 左右；完整计数以
当次任务链为准），另有 `udm_liro`。这些线程大多处于
`TASK_UNINTERRUPTIBLE`。

### 5.3 忙等点

有固件时，串口在 `end entry_main` 之后完全静默，shell 也不响应。
通过 GDB 中断客户机，当前 PC 为：

```
PC = 0x63a1d294   (LIRO 固件内，Thumb)
SP = 0x6747ff78   (内核栈)
```

对应 Thumb 代码：

```
63a1d28e: ldr r0, [pc, #0x16c]   ; r0 = 0xf2a1102c
63a1d290: bl  0x63d6c97a         ; r0 = *(u32 *)r0
63a1d294: tst.w r0, #2
63a1d298: beq 0x63a1d28e         ; 等 bit1
```

之前的初始化还写了：

```
0xf2a11020 |= 0x100
0xf2a11024  = 1
```

`0x63d6c97a` 只是 `ldr r0, [r0]; bx lr`，即一次 MMIO 读。QEMU 的
`cxd90045` 机器没有建模 `0xf2a1xxxx` 块，读回 0，于是固件在
SCHED_FIFO/RT 线程里无限循环，抢死 console、shell 和 Appframework。

### 5.4 跳过单个等待的负结果

把 `0x63a1d298` 的 `beq`（字节 `f9 d0`）改成 NOP（`00 bf`），文件偏移
`0x457298`，得到 `av-cam-nowait1.bin`（SHA-256
`d301162b3e0564410b9d679be8dc5ee3506c3ee0cbbc168303ef98204f3578f3`）。
`entry_main` 仍能返回，但系统依旧静默，说明后面还有同类硬件等待；
单点跳过不是解决方案。

脚本：`scripts/experimental/patch-liro-nowait.py`（仅用于离线 QEMU
实验，严禁用于真机固件）。

### 5.5 对照

不预加载固件时，`liro.ko` 打印：

```
Magic unmatched 0 != 4c49524f
tsk_entry_1st() error
```

随后系统继续启动，`im.elf` 运行到 63 线程。可见静默是 **LIRO 固件
忙等**导致的，不是新内存模型或 63 线程应用本身的问题。

---

## 6. 应用与 UI 当前状态

在真实内存模型、不预加载 LIRO 固件的运行中：

- `im.elf`：63 线程，`VmRSS ≈ 23 MiB`；
- 已映射：`libmpr.so`、`libmprctrl.so`、`libmprdummy.so`；
- **未映射**：`viewUnified8.so`、`libgrm.so`、`libuxengine.so`、`libDDS.so`；
- `bootin.elf`：3 线程，wchan 为 `osal_rcv_msg_tmo`，即阻塞在 UIPC 收消息；
- `/proc/fb` 不存在，`dmpgles2` 的 128 KiB 缓冲几乎全 0。

因此原生菜单还缺三层：

1. **LIRO/CP 硬件块**：至少要让 `0xf2a1102c` 一类状态寄存器按固件预期
   返回，或继续定位/绕过全部忙等点；
2. **UIPC 握手**：`bootin.elf` 需要 CP/LIRO 侧的消息才能继续，之后
   Appframework 才可能加载 UI 库；
3. **Sugilite 显示链路**：没有 GPU/显示/面板模型，就没有可输出的像素。

---

## 7. 复现与判据

### 7.1 QEMU 侧

使用当前 `patches/qemu-a7m3.patch` 构建 `cxd90045` 机器。启动后可在
相机 root shell 中核对：

```sh
# 1. 内核内存地图
cat /proc/meminfo | sed -n '1,6p'

# 2. 非缓存地址表（内核 VA 0x604e07e0 -> PA 0x004e07e0）
busybox dd if=/dev/mem bs=1 skip=$((0x004e07e0)) count=20 2>/dev/null \
  | busybox xxd -p
# 期望：00000014 00000080 00000074 000000a0 ffffffff

# 3. Appframework 状态
pidof im.elf
ls /proc/$(pidof im.elf)/task | wc -l          # 63
cat /proc/$(pidof im.elf)/maps | grep -E 'libmpr|viewUnified|libgrm'

# 4. bootin 的等待点
cat /proc/$(pidof bootin.elf)/wchan            # osal_rcv_msg_tmo
```

### 7.2 LIRO 实验

```sh
# 仅离线 QEMU；固件文件需自行从 nflasha3 提取，不随仓库分发
qemu-system-arm -M cxd90045 ... \
  -device loader,file=$HOME/firmware/a7m3/av-cam.bin,addr=0x035c6000,force-raw=on
```

判据：

- `entry_pre`/`entry_main` 均返回；
- `arch_phys_to_noncache` 不再出现 `0x8bdc12xx` 报错；
- 若串口随后静默且 GDB 显示 PC 在 `0x63a1d2xx`，即命中本文的忙等点。

---

## 8. 未解项与下一步

1. 定位 `av-cam-nowait1.bin` 之后的下一处忙等 PC，统计 LIRO 初始化一共
   需要哪些硬件状态位。
2. 给 `0xf2a1xxxx` 做最小寄存器模型：先实现“写入保存、读回保存值、
   `+0x2c` 返回 bit1”的可控状态，再根据固件后续行为逐步修正。不要一开始
   做完整 CP。
3. 检查 `bootin.elf` 的 UIPC fd/队列与 LIRO 侧端点，确认它等待的具体
   消息；必要时构造最小握手。
4. 显示链路（Sugilite/panel/ramfb）另立任务；参考 A7C2 full-system 的
   ramfb 经验，但 A7M3 是 ARM32 + 不同图形栈，不能直接复用。
5. 上述内容稳定后，再把脚本和判据同步到 `STATUS.md`/`DESIGN.md`。

## 9. 相关文件

- `../patches/qemu-a7m3.patch`：当前 QEMU 模型（DDR1 256 MiB + 真实 cmdline）。
- `../scripts/experimental/patch-libmpr-dfb16.py`：DFB 绕过（宿主机副本）。
- `../scripts/experimental/patch-liro-nowait.py`：LIRO 单点等待跳过（负实验）。
- `../firmware/FINDINGS.md` §3：两份 `kemco.txt` 的区别与完整参数。
- `../STATUS.md`：当前状态摘要。
