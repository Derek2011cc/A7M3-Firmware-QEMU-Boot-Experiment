# A7M3 原生 UI / GPU 渲染实验

在 QEMU 的 `cxd90045` 机器上，把 ILCE-7M3（固件 4.03/4.04 用户态）的**原生菜单视图**
真正打开，并把它提交给 Sony Sugilite GPU 的绘制命令流抓下来、解释并软件重放。

**当前状态**：菜单视图可以打开（`viewSettingMenu.uxc` 已映射），App 会提交 2D/GPU
绘制作业；我们已经解出命令字的基本语法，并在 QEMU 里实现了一个软件解释器，能把
App 自己的命令合成成 640×480 的 RGBA4444 帧。**但像素还不正确**：文字不可读，
纹理采样、shader/顶点语义、坐标缩放仍未解出。这不是可交互的相机模拟器。

本目录是 [A7M3 QEMU 启动实验](../README.md) 的延续；启动链、镜像和内核补丁仍以
`a7m3/` 上级目录为准，这里只放“打开菜单并显示像素”的研究。

## 结果一览

| 能力 | 状态 | 判据 |
|---|---|---|
| 在 `im.elf` 进程内执行探针 | 已通过 | `/usr/bin/scenario.elf` 加载 PA `0x10000000` 的 8 KB 插件，日志 `SCN enter`/`SCN done` |
| 打开原生菜单视图 | 已通过 | `viewManagerIf::openView("view/BASE_MENU", 0)` 返回，运行时 maps 出现 `/usr/share/app/viewSettingMenu.uxc` |
| 确认绘制走 GPU | 已通过 | `GraphicsImage::clear/copy`、`GRM_gpermFillrect/Rectblit` 钩子在整场运行中 0 次调用；2D submit `0x73a100` 15 次调用 |
| GPU 命令流抓取 | 已通过 | QEMU Sugilite 模型在 `0x1800` kick 时 dump 命令缓冲；42 个命令块 |
| 命令语法/地址编码 | 部分通过 | `0x0112` 表面 `<<4`、`0x0589` 纹理 `<<3` 与 LZ77 目的地址精确吻合 |
| 软件重放为帧 | 部分通过 | 逐绘制输出 640×480 RGBA4444 `frame_XXXX.raw`；`artifacts/` 有 PNG/GIF |
| 菜单文字/图标可读 | **未通过** | 纹理采样/shader/顶点语义未解 |
| 实时窗口 | 未实现 | 当前 QEMU 构建只有 `none`/`curses` 后端，用 QMP/文件导出 |

## 快速开始（概要）

前提：已经按 [../BUILD.md](../BUILD.md) 构建了 `cxd90045` QEMU、eMMC 镜像和
`vmlinux-console.bin`/`initrd-qemu.img`；并准备一个带 `/etc/shim_io.so` 预加载的镜像
（见 [scripts/guest/README.md](scripts/guest/README.md)）。

```sh
# 1. 编译 guest 插件（在 WSL/Linux 主机）
bash scripts/guest/build-plugins.sh /path/to/output

# 2. 给 QEMU 打上软件渲染补丁并重编译（补丁基于 ma1co/qemu 的 bionz.c）
cd /path/to/qemu-ma1co
git apply /path/to/ui-rendering/qemu/qemu-bionz-softrender.patch
make -C build-arm -j8

# 3. 启动并打开菜单（路径按自己的环境）
A7M3_LIRO_AUTOREADY=1 \
A7M3_SEND_SO=/path/to/output/vui.so \
A7M3_SUGI_CMDS=1 A7M3_SUGI_EXEC=1 A7M3_SURF_DIR=/tmp/a7m3-frames \
python3 scripts/boot/a7m3-shload.py \
  /path/to/qemu-emmc-2g.sen.img demo scripts/boot/a7m3-cmds-ui1.txt 30

# 4. 把帧转成 PNG/GIF
python3 scripts/analysis/gpu/frames2gif.py /tmp/a7m3-frames /tmp/a7m3-out
```

更完整的构建与复现判据见 [UI-RENDERING.md](UI-RENDERING.md) 和
[HANDOFF-AI.md](HANDOFF-AI.md)。

## 目录结构

```text
ui-rendering/
├── README.md                 本文件：入口、状态、快速开始
├── UI-RENDERING.md           技术报告：证据、命令语法、软件渲染、未解问题
├── EXPERIMENTS.md            实验记录：uiprobe 运行时间线、失败/否定结果
├── HANDOFF-AI.md             给接手 AI 的工作交接：环境、路径、下一步
├── requirements.txt          宿主机 Python 依赖（Pillow/numpy/capstone）
├── scripts/
│   ├── boot/                 启动 harness、QMP/gdbstub 观察、guest 命令文件
│   ├── guest/                guest 插件与预加载 shim 源码（含编译脚本）
│   ├── analysis/             GPU 命令/LZ77/纹理/帧的离线分析
│   └── fat/                  eMMC 镜像内 FAT12 分区的读写
├── qemu/                     QEMU Sugilite 模型补丁与对照源码
├── payload/                  已构建的 guest 插件/shim（附 SHA-256）＋构建说明
├── artifacts/                软件重放产物（PNG/GIF）＋说明
└── evidence/                 关键日志摘录（命令流、LZ77、maps、gdbstub）＋说明
```

## 关键结论（先看这几条）

1. **菜单不是 CPU 画的**。四个 CPU 图形钩子（`GraphicsImage::clear/copy`、
   `GRM_gpermFillrect/Rectblit`）整场 0 调用；App 通过 `0x73a100` 提交 2D 作业，
   通过 `0x1804`/`0x1800` 提交 GPU 命令缓冲。
2. **命令字格式**是 `(opcode<<16)|0xf000` + 一个 value（部分 op 后跟多字）。
   `0x0112` 的表面地址要 `<<4`，`0x0589` 的纹理地址要 `<<3`；
   两者都与 LZ77 解码目的地址精确吻合。
3. **LZ77 资源是 shim 解码的**：相机 `/dev/ldec` 硬件未建模，shim 在
   `ioctl(fd, 1, arg)` 里用 Sony LZ77 解码，再通过 `/dev/mem` 写到物理目的地址。
4. **软件重放已经能出帧**，但只是“结构上像 UI”：橙色砖纹条、顶部图标行、底部面板
   都能看到；文字/小图标仍不可读。下一步是找命令构建函数和 shader/顶点语义，
   而不是继续猜坐标。

## 安全与边界

- 本目录的所有实验都在 **QEMU + 镜像副本** 上完成，**不接触真实相机、不刷固件、
  不修改只读分区**。QEMU 驱动使用 `snapshot=on`，所有写入不落盘。
- 不要把这些命令流/纹理地址当成真机物理地址使用；它们是 QEMU 模型中的客户机地址。
- Sony 固件、分区转储、`vmlinux`/`initrd`、`*.img` 和大体积运行日志**不随仓库分发**；
  需要使用者自行准备。仓库内的 `evidence/` 只放关键日志摘录。
- 相机侧脚本/字符串必须保持 ASCII；本目录的 guest 插件源码均为 ASCII。

## 相关文档

- [../README.md](../README.md)：A7M3 QEMU 启动实验总览。
- [../BUILD.md](../BUILD.md)：内核、镜像、QEMU 的构建与复现。
- [../STATUS.md](../STATUS.md)：启动链的已核验结果与历史阻塞点。
- [../docs/GPU-BYPASS.md](../docs/GPU-BYPASS.md)：Sugilite GPU 卡点的历史记录。
- [UI-RENDERING.md](UI-RENDERING.md)：本成果的完整技术报告。
- [EXPERIMENTS.md](EXPERIMENTS.md)：按运行顺序的实验记录与否定结果。
- [HANDOFF-AI.md](HANDOFF-AI.md)：接手工作的环境与下一步清单。

## 许可

文档与脚本遵循仓库根目录的 `LICENSE`（MIT）。Sony、ILCE、BIONZ 等名称与固件归
Sony 所有；本目录不包含 Sony 二进制。
