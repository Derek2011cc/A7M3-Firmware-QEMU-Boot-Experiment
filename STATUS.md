# 状态：ILCE-7M3 固件在 QEMU 中的启动

目标：在 ma1co/qemu 的 `cxd90045` 机器上离线启动 A7M3 原厂固件，尽可能走到原生
用户态（最终目标是菜单/拍摄链）。

> **2026-11-01 更新（原生 UI / GPU 渲染）**：在 `im.elf` 进程内注入探针并调用
> `viewManagerIf::openView("view/BASE_MENU", 0)` 后，`viewSettingMenu.uxc` 会加载，
> App 通过 `0x73a100` 提交 2D 作业、通过 `0x1804`/`0x1800` 提交 GPU 命令；
> 命令流已抓取并在 QEMU 内软件重放成 640×480 帧。**像素仍不正确**（文字不可读，
> shader/顶点/纹理采样未解）。详见
> [ui-rendering/README.md](ui-rendering/README.md) 与
> [ui-rendering/UI-RENDERING.md](ui-rendering/UI-RENDERING.md)。
> 下面 2026-10-31 的启动链结论仍然有效，但“UI 库未加载/菜单未完成”针对的是
> 当时未注入探针的启动路径。

**当前结论（2026-10-31 晚）**：

- 内核与相机 rootfs 用户态可以完整启动到 BusyBox root shell；
- 绕过 DFB/`nflasha4` 崩溃后，Appframework `im.elf` 可以稳定运行到 **63 线程**；
- 预加载 `av-cam.bin` 后，LIRO 协处理器固件能在 QEMU 中执行
  `entry_pre`/`entry_main`，但随后会在未建模的 `0xf2a1xxxx` 硬件块上忙等，
  抢死 Linux；
- （2026-10-31 路径）原生菜单/UI/显示未完成：UI 库未加载、`bootin.elf` 阻塞在
  UIPC、没有 Sugilite 显示模型；2026-11-01 的探针路径见上面的更新。

这不是可交互的相机模拟器。最新细节见
[docs/LIRO-AND-MEMORY.md](docs/LIRO-AND-MEMORY.md)。

## 已核验（有串口日志/读回证据）

| 项目 | 结果 | 判据/说明 |
|---|---|---|
| 内核启动 | 通过 | Linux `3.0.27_nl-rt106+`，ARM32；QEMU 串口完整内核日志 |
| 串口控制台 | 通过 | ttyAM0 PL011 输出；需内核补丁绕过 Sony ADC/errind 门控（见 DESIGN.md §5） |
| ATAGS / 机器号 | 通过 | `r1=0x11a`；ATAGS 位于 PA `0x00010000` |
| 内存模型 | 通过（真实主内核参数） | DDR0 1 GiB + DDR1 256 MiB @ `0x80000000`；`memrsv` 含 704M/14M/242M `uc` |
| eMMC 识别 | 通过 | `/proc/partitions` 出现 `nflasha1..nflasha25` 与 `nflashaB0/B1` |
| 分区挂载 | 通过 | `/`、`/usr`、`/tmp`、`/etc`、`/system`、`/setting`、`/log`、`/lens`、`/cert` 均挂载成功 |
| 相机内核模块 | 部分通过 | devno/dmac/ldec/emmc/backup/dmm/upm 等加载；`backup.ko` 需 `Backup_load=1`，`udate.ko` 需跳过，`grm_gles.ko` 需时钟绕过（见下） |
| root shell | 通过 | 相机 rootfs 的 `/bin/ash`，提示符 `/ #` |
| Appframework 启动 | 通过（实验绕过） | DFB 目标改为 `/dev/nflasha16` 后，`im.elf` 稳定运行，63 线程，`libmpr` 已映射 |
| LIRO 固件执行（预加载后） | 部分通过 | `entry_pre`/`entry_main` 返回；创建至少 134 个 `liro-kliro_*` 线程；随后忙等 `0xf2a1102c` bit1 |
| 原生菜单/UI（2026-10-31 路径） | **未通过** | `viewUnified8.so`/`libgrm.so`/`libuxengine.so`/`libDDS.so` 未加载；`bootin.elf` 阻塞在 `osal_rcv_msg_tmo` |
| 原生菜单视图 + GPU 命令（2026-11-01 路径） | 部分通过 | 注入探针后 `openView("view/BASE_MENU")` 成功；`viewSettingMenu.uxc` 映射；15 次 2D submit、48 次 GPU kick；命令流可软件重放成帧 |
| 菜单像素正确显示 | **未通过** | 文字/图标不可读；shader/顶点/纹理采样未解（见 [ui-rendering](ui-rendering/README.md)） |
| 显示 | **未通过（实时）** | 无 `/proc/fb`；`dmpgles2` 128 KiB 缓冲几乎全 0；QEMU 无 Sugilite/panel 模型；仅有离线帧 |

## 关键发现

### 1. 主内核的真实启动参数

不在 `nflasha1/BOOT/KEMCO.TXT`（815 B，updater/recovery），而在
**`nflasha2:/kemco.txt`**（1,901 B，SHA-256
`83b32a57278ee640e27fb60cbeec28a9c74b6055f5fe1b2b728d1c053b5988c8`）。
内存部分：

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

- DDR1 是 **256 MiB**（不是 updater 的 1016 MiB），且 14M+242M `uc` 覆盖全段；
- 低端 `0x14000000..0x3FFFFFFF` 是 704 MiB `uc`；
- LIRO 的 `0x8bdc1200` 落在 DDR1 的 242 MiB `uc` 区间内。

### 2. 非缓存地址表

`parse_memrsv` 只把 `memrsv` 区间登记到内核表 `0x604e07e0`；ATAG/`mem=`
内存 bank 不登记。使用真实参数后，客户机读回该表：

```
00000014 00000080 00000074 000000a0 ffffffff
```

即 `{0x14000000, 0x80000000, 0x74000000, 0xa0000000, 0xffffffff}`。
`arch_phys_to_noncache(0x8bdc1200)` 因此返回 `0xabdc1200`，LIRO 之前
129 条 `arch_phys_to_noncache` 报错全部消失。

### 3. DFB/`nflasha4` 崩溃与绕过

`libmpr.so` 的 DFB 初始化打开 `/dev/nflasha4`；合成 MBR 中该分区只有
1 个扇区，`DFA::FlashWrite` 的 `lseek` 失败后，错误路径二次加锁
MemInfo 互斥量，触发 glibc 2.11.2 `pthread_mutex_lock.c:62` 断言。
DFB 失败是上游原因；早先看到的 `NULL MemInfo`/`__pthread_mutex_lock(0x1c)`
崩溃是其下游表现之一，而错误路径的二次加锁断言是直接触发点。

绕过：把 `libmpr.so` 中的 DFB 打开路径改为 `/dev/nflasha16`（140 MiB
全零分区）。补丁位置与字节见
[docs/LIRO-AND-MEMORY.md](docs/LIRO-AND-MEMORY.md) §4 和
`scripts/experimental/patch-libmpr-dfb16.py`。这是实验性绕过，不是硬件模拟。

### 4. LIRO 固件忙等

固件（`av-cam.bin` 4.03，18,449,784 B，SHA-256
`e1b0f84e8323005a34b43de6dd0083fca712d2c654f0d9716512e0a54631bbe7`）
执行完 `entry_main` 后，在 Thumb 代码 `0x63a1d294` 循环：

```
ldr r0, [0xf2a1102c]
tst r0, #2
beq  0x63a1d28e
```

QEMU 未建模 `0xf2a1xxxx`，读回 0，固件在 RT 线程中死循环。跳过一次
等待（`av-cam-nowait1.bin`）仍会停在后续等待，说明需要成组建模或
系统性定位。

## 当前阻塞点

1. **LIRO/CP 硬件块 `0xf2a1xxxx` 未建模**（主阻塞）
   - 固件忙等 `0xf2a1102c` bit1；QEMU 读回 0；RT 线程抢死 console/app。
2. **UIPC/CP 握手**
   - `bootin.elf` 3 线程，wchan `osal_rcv_msg_tmo`；
   - UI 库 `viewUnified8.so`/`libgrm.so`/`libuxengine.so`/`libDDS.so` 均未加载。
3. **Sugilite 显示链路**
   - 无 GPU/显示/面板模型；即使应用继续，也没有可输出像素的设备。
   - 2026-11-01：命令流已能抓取并在 QEMU 内软件重放（离线帧），但还没有实时窗口；
     剩余的是命令语义/纹理采样，而不是“完全没有像素”。见
     [ui-rendering/UI-RENDERING.md](ui-rendering/UI-RENDERING.md)。

## 未确认项

| 项目 | 状态 |
|---|---|
| 主内核 `kemco.txt` 的完整参数是否还有遗漏 | 已取内存相关行；其余参数（`wbi_memclr`/`wbimem`/`pm.*`）尚未逐项模拟 |
| 真机 `/proc/iomem` | 未核对 |
| `nflasha4` CMMeX 真实内容 | 未解；当前用 `/dev/nflasha16` 绕过 |
| B0/B1 分区内容与用途 | B0 是 EXBL `V85.00.03` 引导镜像；B1 全零；EXBL 内部结构未解析 |
| 分区 8/9/14 | 设备节点不存在（Senser 130） |
| `0xf2a1xxxx` 寄存器语义 | 仅知 `+0x2c` bit1 是初始化等待位；其余寄存器用途未知 |
| 显示、传感器、拍摄链 | 未开始 |

## 关键数字（便于复核）

- 内核：`vmlinux.bin`，4,117,760 字节，SHA-256
  `13649dc02443d761df58ba2745171acb089f7865c12fd384df1191e2c45c5cd6`。
- initrd：`initrd-4.03.img`，2,178,048 字节，SHA-256
  `0791900525b4e5040edd0b233fb513bc242a3480eca88feac5c355410def9d10`。
- 内核加载 PA：`0x00108000`；initrd PA：`0x00700000`；ATAGS PA：`0x00010000`。
- 机器号：`0x11a`；内核链接基址 `0x60108000`；`PAGE_OFFSET=0x60000000`。
- QEMU 机器：`cxd90045`；DDR0 `0x00000000` + 1 GiB；DDR1 `0x80000000` + 256 MiB。
- 合成 eMMC：2,148,007,936 字节 = `0x80000` 前缀 + 2 GiB 用户区。
- LIRO 固件加载：PA `0x035c6000` / VA `0x635c6000`；入口
  `0x635c6044`/`0x635c6058`；忙等 PC `0x63a1d294`。
- 非缓存表：内核 VA `0x604e07e0`（PA `0x004e07e0`）。

完整产物与哈希见 [docs/ARTIFACTS.md](docs/ARTIFACTS.md)。

## 如何判断“启动成功”

1. 串口出现内核日志，并包含 `Memory: 1024MB 256MB = 1280MB total`。
2. `/proc/partitions` 中出现 `nflasha1` … `nflasha25`。
3. `/proc/mounts` 中 `/usr`、`/system`、`/setting` 等均已挂载。
4. 最终出现 `/ #`（若落入 `Exception>`，输入 `exit` 返回 shell）。
5. 在 `/ #` 下执行 `cat /proc/version`、`ls /usr/bin` 应正常返回。
6. 在打了 DFB 补丁的副本中，`pidof im.elf` 有值，
   `ls /proc/$(pidof im.elf)/task | wc -l` 为 63。

只看到内核日志而没有 `/ #`，通常意味着某个模块在 init 阶段失败；用
`initcall_debug` 日志和串口最后几行定位。

## 下一步（按性价比排序）

1. **给 `0xf2a1xxxx` 做最小寄存器模型**：先实现写入保存/读回、`+0x2c`
   返回 bit1 的可控状态，观察 LIRO 是否继续；再按后续等待逐步修正。
   不要一开始做完整 CP。
2. **定位 nowait1 之后的下一处忙等 PC**，统计 LIRO 初始化需要的硬件
   状态位。
3. **检查 `bootin.elf` 的 UIPC 队列**，确认它等待的消息；必要时构造最小握手。
4. **显示模型另立任务**：参考 A7C2 full-system 的 ramfb 经验，但 A7M3 是
   ARM32 + Sugilite，不能直接复用。当前已在
   [ui-rendering/](ui-rendering/README.md) 做到“命令抓取 + 软件重放”，下一步是
   命令构建函数和 shader/顶点语义。
5. 上述内容稳定后，把结果同步到 DESIGN.md 与 docs/GPU-BYPASS.md。

## 复现

见 [BUILD.md](BUILD.md)。最短路径：

```sh
bash scripts/build-emmc.sh
bash scripts/run-shell.sh
# 看到 Exception> 后输入 exit
```

当前 `patches/qemu-a7m3.patch` 已包含 DDR1 256 MiB、第二个 ATAG_MEM 和
主内核真实 `memrsv` 命令行。实验性的 DFB/LIRO 补丁脚本在
`scripts/experimental/`，只作用于宿主机副本，不接触真机。
