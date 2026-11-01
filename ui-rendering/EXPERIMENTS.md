# 实验记录（2026-10-31 ～ 2026-11-01）

本页按时间顺序记录 `a7m3-uiprobe*` 运行做了什么、看到了什么，便于接手者快速定位
“哪个日志对应哪个结论”。原始日志在工作区 `a7m3-qemu-logs/`，不随仓库分发。

约定：

- 所有运行都用 `scripts/boot/a7m3-shload.py` + `scripts/boot/a7m3-cmds-ui1.txt`；
- 插件经 `A7M3_SEND_SO` 加载到 PA `0x10000000`，guest dd 到 `/tmp/v.so`；
- `A7M3_LIRO_AUTOREADY=1` 始终开启；
- 坐标/命令 dump 的环境变量见 `scripts/boot/README.md`。

## 1. 时间线

| 运行 | 插件 | 主要开关 | 结果 |
|---|---|---|---|
| `uiprobe1` | `scn_ui` 系列 | — | 确认能在 `im.elf` 内执行插件；钩子日志 `UIINFO/UIINST/UIDONE` 各 1 条，但没有 `UI2D`（菜单尚未真正绘制） |
| `uiprobe2` | `vui.so`（早期版本） | `SUGI_CMDS=1` | 绘制期间 maps 出现 `/usr/share/app/viewSettingMenu.uxc`；48 次 H2D/kick（早期 `vui` 也打印 H2D/HKICK）；CPU 钩子（clear/copy/fill/rect）0 调用 |
| `uiprobe3` | `vui.so` | `SUGI_CMDS=1` | 42 个 `SUGI-CMDA` 命令块；解出 `0x0112/0x0589` 等 opcode；LZ77 目的地址 |
| `uiprobe4` | `vui.so` | `SUGI_CMDS=1` | `SUGI-TEX/DST/SRC` 纹理/表面 dump；确认 `0x0589<<3` 与 LZ77 目的地址吻合 |
| `uiprobe5/6` | `vtex.so`/`vui.so` | — | 尝试直接读 `0x0445c040` 等 VA：映射内容不稳定/为 0；pagemap 路线不可用 |
| `uiprobe7/8/9/10/11` | `vui.so` | `SUGI_EXEC=1` + QMP | QMP `pmemsave` 0x202e0200/0x222e0200/0x226e0000/0x22770000/0x228f0000/0x2055f000/0x20530000；首批软件重放结果 |
| `uiprobe12` | `vpg.so` | — | **失败**：VA 采样导致 `THREAD_INFRA0` translation fault（`0x0445d000`），App 崩溃；之后不再用 vpg 长跑 |
| `uiprobe13` | `vui.so` | `SUGI_EXEC=1` | 首批逐绘制帧（`/tmp/a7m3-frames`）；UI2D=15、UICLR/UICPY/UIFILL/UIRECT=0；contact sheet/GIF |
| `uiprobe14/15` | `vui.so` | 变体 | 重复/对照，确认帧与 UI2D 记录可复现 |
| `uiprobe16` | `vui.so` | `SUGI_CENTER=1` | 居中坐标模式 60 帧（`/tmp/a7m3-frames-center`），`artifacts/c60-*` |
| `uiprobe17` | — | — | **启动失败**：`.log` 为 0 B，QEMU 只输出 PL011 初始化后被 harness 终止 |
| `uiprobe18` | `vhk2.so` | `-gdb` | `Z2` 首次命中：命令环页第一次写来自 libc 拷贝/清零（PC `0x045bb2f0`） |
| `uiprobe19` | `vhk2.so` + maps | `-gdb` | 动态 VA 观察点命中 libc-2.11.2 `0x04616350`，写入 `0x0cdcaa70`；构建者仍未抓到 |

## 2. 每个阶段的关键判据

### 2.1 “菜单确实打开了”

- `viewManagerIf::openView("view/BASE_MENU", 0)` 返回；
- 绘制期间 `/proc/self/maps` 有 `viewSettingMenu.uxc`；
- 2D submit 15 次，GPU kick 48 次。

### 2.2 “不是 CPU 画的”

`scn_ui.c` 的四个 CPU 图形钩子（`GraphicsImage::clear/copy`、
`GRM_gpermFillrect/Rectblit`）在所有 `vui.so` 运行里**整场 0 调用**；
`UI2D` 记录 15 条。见 `evidence/04-guest-hooks.txt`。

### 2.3 “命令流是 UI 绘制”

- 42 个命令块里有 15 个带纹理绑定；`0x0589<<3` 与 LZ77 目的地址精确吻合；
- 矩形 `0x0047/0x0048` 随帧变化，形成滑动动画；
- 软件重放能出结构化的帧（橙色条、图标行、底部面板），不是随机噪声。

### 2.4 “为什么文字还不可读”

- 命令包里的 `0x0202` 后跟嵌套命令列表（`0x0458/0x0459` 属性、`0x0460/0x0461`
  常量），当前软件光栅完全忽略；
- 纹理格式/tiling 未定；
- 纹理内存运行时复用，事后 QMP dump 不等于绘制时刻内容。

## 3. 失败/否定记录（保留，避免重复）

| 尝试 | 结果 | 原因 |
|---|---|---|
| 直接用 `0x0445c040` 等 VA 读纹理 | 内容为 0 或不稳定 | 那是 App 的匿名映射，不是物理纹理 |
| `pagemap` + VA 采样（`vpg.so`） | App 崩溃（`0x0445d000` translation fault） | 映射在稍后失效；**不要用于长跑** |
| 半坐标模式（`A7M3_SUGI_HALF=1`） | 内容缩到左上 1/4 | 坐标假设错误或至少不完整 |
| 固定 VA gdbstub 观察点 | 从未命中 | libObj 基址/命令环 VA 每次启动都变（ASLR） |
| gdbstub 首次命中后直接继续 | 反复命中同一条 memset，VM 卡住 | 需要在命中后 `z2` 删除或单步越过 |
| 把 `0x0203` 当平坦顶点数组 | 采样结果不对 | 它其实是 16-bit 索引缓冲，属性在嵌套命令列表里 |
| 事后 QMP dump 纹理当绘制内容 | 不同实验截图矛盾 | 纹理内存在运行时复用 |

## 4. 复现某个阶段

以“命令流 dump”为例：

```sh
A7M3_LIRO_AUTOREADY=1 \
A7M3_SEND_SO=$PWD/payload/vui.so \
A7M3_SUGI_CMDS=1 A7M3_SUGI_EXEC=1 \
A7M3_SURF_DIR=/tmp/a7m3-frames A7M3_PUMP=100 \
python3 scripts/boot/a7m3-shload.py \
  /path/to/qemu-emmc-2g.sen.img ui \
  scripts/boot/a7m3-cmds-ui1.txt 30
```

日志在 `$A7M3_LOGDIR/a7m3-ui.log` 与 `.err`。

gdbstub 版本：在 `A7M3_QEMU_EXTRA="-gdb tcp::1234"` 下启动，再运行
`scripts/boot/rsp_kick2.py <log> 300 /tmp/rsp.log`。
