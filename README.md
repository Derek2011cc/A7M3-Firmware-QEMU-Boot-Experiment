# A7M3-Firmware-QEMU-Boot-Experiment

**项目有借助大量 AI 工具，请注意辨别**
QQ 交流群：736434809

在 ma1co/qemu 的 `cxd90045` 机器上启动 **ILCE-7M3（A7M3）** 相机固件
（Linux 3.0.27_nl-rt106+，ARM32 Cortex-A5）的离线实验记录、补丁与复现脚本。

本仓库是研究主仓库：启动链、QEMU 机器模型、固件/分区分析、服务模式、隐藏功能与
原生菜单渲染实验。**开箱即用的运行版本已拆到子仓库**（见下）。

## 子仓库与相关仓库

| 仓库 | 作用 |
|---|---|
| **[a7m3-QEMU-shell](https://github.com/Derek2011cc/a7m3-QEMU-shell)** | **本仓库的“下载即用”子仓库**：包含内核、initrd、压缩分片的 2 GiB eMMC 镜像、预编译 QEMU 和浏览器串口终端；`run.cmd` / `./run.sh web` 直接进入 A7M3 虚拟机 shell，不需要自己准备固件或编译。 |
| [a7m3-connect-shell](https://github.com/Derek2011cc/a7m3-connect-shell) | 服务模式（Senser）连接工具：真机 root shell、`cmd`/`push`/`pull`，可用于只读分区转储，为 QEMU 镜像准备输入。 |

如果你只想**尽快使用 A7M3 虚拟机 shell**，请直接去
[a7m3-QEMU-shell](https://github.com/Derek2011cc/a7m3-QEMU-shell)；
本仓库保留研究过程、源码级补丁和更完整的技术记录。

## 当前状态（2026-11）

- 内核与相机用户态可启动到原固件 **BusyBox root shell**（串口控制台可用）；
- 绕过 DFB/`nflasha4` 崩溃后，Appframework（`/usr/bin/im.elf`）可稳定到 **63 线程**；
- LIRO 固件可执行 `entry_pre`/`entry_main`，随后在未建模的 `0xf2a1xxxx` 上忙等；
- 原生菜单视图可在 `im.elf` 内打开，GPU 命令流已抓取并在 QEMU 内软件重放为
  640×480 帧；**像素仍不正确**（文字不可读，shader/顶点/纹理采样未解）；
- **服务模式（Senser）已在真机验证**：两阶段认证后可进入相机 BusyBox shell，
  `readHasp()` 返回 88 字节；
- **工厂模式入口已找到**：`/scenario/MPR_SCN_SET_FACTORY_MODE.so`，但 payload 语义
  与生效时机仍未完全解出。

| 能力 | 状态 |
|---|---|
| 原内核启动（3.0.27） | 已通过 |
| 串口控制台（ttyAM0） | 已通过（需绕过 Sony ADC/errind 门控） |
| 内存模型（DDR0 1 GiB + DDR1 256 MiB，主内核 `memrsv`） | 已通过 |
| eMMC 分区 `/dev/nflasha1..25` | 已通过（合成 MBR） |
| 分区挂载（/ /usr /tmp /etc /system /setting /log /lens /cert） | 已通过 |
| 相机内核模块加载（devno/dmac/ldec/emmc/backup/dmm/upm/...） | 已通过（部分需参数/桩） |
| 相机 rootfs 的 BusyBox root shell | 已通过 |
| Appframework（im.elf）启动、FixedArea mmap、63 线程 | 已通过（实验性 DFB 绕过） |
| LIRO 固件执行 | 部分通过（忙等未建模硬件） |
| 原生菜单视图打开 / GPU 命令抓取 | 已通过（实验） |
| 菜单像素正确显示 | **部分通过**——结构可重放，文字/图标不可读 |
| 服务模式真机 shell | 已通过（PMCA Senser，见子仓库） |
| 工厂模式 | **入口已找到，未确认进入 UI** |
| 实时显示 / 拍摄链 | **未完成** |

A7M3 是 ARM32（CXD90045），UI 强依赖 Sony 私有 GPU（Sugilite），
不能直接复用 A7C II（AArch64、cxd90057）的 runtime。

## 仓库结构

```
README.md                 本文件
BUILD.md                  从干净主机复现“内核 + rootfs 启动到 shell”
DESIGN.md                 启动链、页表/ATAGS、内存模型、eMMC、模块补丁
STATUS.md                 已核验结果、未完成项、证据摘要
LICENSE                   MIT（文档/脚本）
docs/                     技术记录与最新进展
patches/                  QEMU cxd90045 机器模型差分
scripts/                  合成 eMMC、内核/initrd 补丁、启动脚本
firmware/                 分区清单、FINDINGS、隐藏功能分析、工具
ui-rendering/             原生菜单/GPU 命令流/软件重放/真机采集基准
```

## 快速开始

### 只想运行 A7M3 虚拟机 shell

去子仓库 [a7m3-QEMU-shell](https://github.com/Derek2011cc/a7m3-QEMU-shell)：

```powershell
.\run.cmd
```

或 Linux：

```bash
./run.sh web
```

### 从本仓库手动复现

1. 准备你自己的 A7M3 4.03 固件输入（官方更新包或只读服务模式转储）；
2. 用 `scripts/` 提取内核/initrd 并打补丁；
3. `bash scripts/build-emmc.sh` 生成 2 GiB 合成 eMMC；
4. 构建打过 `patches/qemu-a7m3.patch` 的 ma1co/qemu；
5. `bash scripts/run-shell.sh`，出现 `Exception>` 后输入 `exit` 得到 `/ #`。

完整步骤、判据和已知失败点见 [BUILD.md](BUILD.md)。

### 真机服务模式 / 分区转储

使用 [a7m3-connect-shell](https://github.com/Derek2011cc/a7m3-connect-shell)：

```powershell
.\cam-shell.cmd cmd "uname -a"
.\cam-shell.cmd push local.bin /tmp/x.bin
```

## 最新进展

2026-11-01 前后的最新结果（QEMU shell、Senser 服务模式、工厂模式实验、
隐藏功能、真机菜单像素基准）汇总在
[docs/RESEARCH-UPDATES-2026-11.md](docs/RESEARCH-UPDATES-2026-11.md)。

## 文档索引

- [BUILD.md](BUILD.md)：环境、输入准备、构建、运行与复现判据。
- [DESIGN.md](DESIGN.md)：启动链、页表/ATAGS、内存模型、控制台门控、eMMC 与模块补丁。
- [STATUS.md](STATUS.md)：已核验结果、未完成项和证据摘要。
- [docs/RESEARCH-UPDATES-2026-11.md](docs/RESEARCH-UPDATES-2026-11.md)：**最新进展汇总**。
- [docs/ARTIFACTS.md](docs/ARTIFACTS.md)：生成物清单、SHA-256 与“不随仓库分发”说明。
- [docs/GPU-BYPASS.md](docs/GPU-BYPASS.md)：Sugilite GPU 卡点与可选绕过路线。
- [docs/LIRO-AND-MEMORY.md](docs/LIRO-AND-MEMORY.md)：主内核启动参数、内存模型、
  DFB 崩溃根因与 LIRO 忙等。
- [firmware/FINDINGS.md](firmware/FINDINGS.md)：分区地图、两份 `kemco.txt`、
  内核/initrd、模块和未解项。
- [firmware/HIDDEN-FUNCTIONS-2026-11-01.md](firmware/HIDDEN-FUNCTIONS-2026-11-01.md)：
  JIRITSU/MPR 工厂命令、隐藏视图、服务模式与工厂模式实验。
- [firmware/SUMMARY.md](firmware/SUMMARY.md)：2026-10-31 完整实验记录（历史）。
- [firmware/MANIFEST.md](firmware/MANIFEST.md)：17 个分区/启动分区转储清单。
- [patches/README.md](patches/README.md)：QEMU 差分的功能/调试部分说明。
- [ui-rendering/README.md](ui-rendering/README.md)：原生菜单/GPU 渲染实验入口。
- [ui-rendering/UI-RENDERING.md](ui-rendering/UI-RENDERING.md)：GPU 命令语法、LZ77、
  软件重放与未解问题。
- [ui-rendering/FINDINGS-2026-11-01.md](ui-rendering/FINDINGS-2026-11-01.md)：
  2026-11-01 的驱动/命令流结论（§0–§18）。
- [ui-rendering/MILESTONE-2026-11-01.md](ui-rendering/MILESTONE-2026-11-01.md)：
  结构级里程碑与边界。
- [ui-rendering/HANDOFF-AI.md](ui-rendering/HANDOFF-AI.md)：给接手 AI 的环境与下一步。
- [ui-rendering/docs/CAPTURE-REFERENCE.md](ui-rendering/docs/CAPTURE-REFERENCE.md)：
  HDMI 采集、归一化与 diff 工具。

## 风险与边界

- 本仓库脚本只在本机生成/修改 eMMC 镜像、内核副本和 initrd 副本，**不接触真实相机，
  不刷写固件，不修改只读分区**；
- 服务模式转储只能**只读**进行；不要写相机的小分区；
- 只在你自己的设备上获取固件数据；**本仓库不包含** Sony 固件、分区转储、
  `vmlinux`/`initrd`、`*.img` 或运行日志；开箱即用的固件/镜像在子仓库
  [a7m3-QEMU-shell](https://github.com/Derek2011cc/a7m3-QEMU-shell)；
- 本实验修改的是 QEMU 机器模型和固件副本，与真机运行无关。

## 致谢

- [ma1co/qemu](https://github.com/ma1co/qemu) — `cxd90045` 机器与 BIONZ 设备模型；
- [ma1co/Sony-PMCA-RE](https://github.com/ma1co/Sony-PMCA-RE) — 服务模式 USB 协议与 Senser 认证；
- [ma1co/fwtool.py](https://github.com/ma1co/fwtool.py) — Sony 固件包解析；
- [QEMU](https://www.qemu.org/) — 全系统模拟器；
- [usbipd-win](https://github.com/dorssel/usbipd-win) — Windows → WSL USB 直通；
- [a7m3-QEMU-shell](https://github.com/Derek2011cc/a7m3-QEMU-shell) — 本仓库的运行版子仓库；
- [a7m3-connect-shell](https://github.com/Derek2011cc/a7m3-connect-shell) — 服务模式 shell 与转储工具。

## 许可

本仓库文档与脚本遵循 [LICENSE](LICENSE)（MIT）。Sony、ILCE、BIONZ 等名称与固件
归 Sony 所有；本仓库不包含 Sony 二进制。
