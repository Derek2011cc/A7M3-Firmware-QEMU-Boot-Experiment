# A7M3 QEMU 原生菜单：阶段验收与复现（2026-11-01）

> 本文件是接手后的**阶段验收记录**，不是“已完成原生模拟器”的声明。
> 详细技术结论见 [FINDINGS-2026-11-01.md](FINDINGS-2026-11-01.md)、[HANDOFF-AI.md](HANDOFF-AI.md)。

## 1. 已达成（可复现、可核对）

| 项 | 证据/产物 |
|---|---|
| 原固件启动到相机用户态 | 串口 `/ #`；`a7m3-qemu-logs/a7m3-*.log` |
| 打开菜单视图并触发 GPU 绘制 | `view/BASE_MENU`、`view/GLOBAL_MENU`；`SUGI-SD` 60–77 条 |
| QEMU 内软件解释 GPU 命令流 | `qemu/bionz-2026-11-01.c` + `qemu-bionz-center-tex.patch` |
| 坐标变换（已由模板证实） | `pixel=(v+W)/2`，`W=low16(0x0049)`、`H=high16(0x0049)` |
| 命令模板 | libObj vaddr `0x1285aa4`，127 对 `(op,value)` |
| opcode 语义表 | 发射函数 `0x796560` 的 47 个调用点；见 FINDINGS §12 |
| 提交链 | GLES draw `0x797xxx` → `0x739340` → `0x73af20` → `0x73b188` 入队 |
| 绘制 → 屏幕矩形映射 | `artifacts/cs0060-draw-list.txt`、`cs0060-draw-overlay.png` |
| 6 个 tab 的菜单结构 | `artifacts/global-menu-frame61.png`（6 个 tab 位置与实拍数量一致） |
| 分层对照 | `artifacts/menu-skip640.png`、`menu-skipboth.png` |

## 2. 未达成（诚实边界）

- **文字不可读**：命令流里没有找到直观的顶点 UV；`0x0450f026` 是 uniform 上传
  （`invScreenSize`、1/640、1/480），`0x0216` 是 uniform 常量块；`0x0458/0x0459`
  疑似 shader 二进制，尚未解释。
- **tab 颜色/图标内容**：所有 draw 的 `0x0105` 都是 `0xff00ff70`，per-tab 颜色不在
  命令流里；混合状态 `0x010b..0x010f` 未实现。
- **像素级正确性**：当前帧是“命令流驱动的结构近似”，不是逐像素还原；与实拍相比，
  tab 方向/颜色/文字仍不同。

## 3. 复现（最短路径）

```sh
# 1) QEMU（已打补丁，见 qemu/ 目录）
make -C <qemu>/build-arm -j8

# 2) 启动并打开 BASE_MENU（或 GLOBAL_MENU：payload/vopen2.so）
A7M3_LIRO_AUTOREADY=1 \
A7M3_SEND_SO=<repo>/a7m3/ui-rendering/payload/vui.so \
A7M3_SUGI_CMDS=1 A7M3_SUGI_EXEC=1 A7M3_SUGI_CENTERSCALE=1 \
A7M3_SURF_DIR=/tmp/frames A7M3_TEX_DIR=/tmp/tex A7M3_PUMP=100 \
python3 scripts/boot/a7m3-shload.py \
  <IMG> menu scripts/boot/a7m3-cmds-ui1.txt 30

# 3) 帧/纹理转 PNG
python3 scripts/analysis/gpu/tex_convert.py /tmp/tex/tex_0001_640x480.raw out.png
python3 scripts/analysis/gpu/draw_overlay.py --err <run>.err --raw /tmp/frames/frame_0060.raw \
  --out overlay.png --list draws.txt
```

新增开关：`A7M3_SUGI_CENTERSCALE`（按 0x0049 的 W/H 居中 + 按矩形缩放）、
`A7M3_SUGI_HYBRID`（大矩形平铺）、`A7M3_TEX_DIR`（绘制时刻纹理转储）、
`A7M3_SUGI_SKIP_TEX=<hex[,hex]>`（按纹理 PA 跳过指定层做对照）。

## 4. 结论

**已交付一个“命令流驱动、几何可核对”的原生菜单结构帧**：每个可见区域都能回溯到
App 自己提交的 draw（矩形/纹理/层），坐标变换由命令模板证实，6 个 tab 的数量与实拍
一致。**尚未达到像素级原生菜单**：文字与 tab 内容仍受 shader/UV/混合/纹理采样限制。

下一步优先级：① 定位/解释 `0x0458/0x0459`（shader 或顶点流）；② 实现
`0x010b..0x010f` 混合；③ per-tab 颜色 uniform；④ 用实拍图做逐区域像素对照。
