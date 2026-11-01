# 证据摘录

这里只放**关键日志摘录**（纯文本），不放大体积原始日志/Sony 二进制/镜像。
原始日志在工作区的 `a7m3-qemu-logs/`，QMP raw 在 `/tmp/a7m3-pm-*.raw`，
均不随仓库分发。

| 文件 | 证明什么 |
|---|---|
| `01-menu-view-and-draws.txt` | 菜单绘制期间 `viewSettingMenu.uxc`/`viewUnified2.so` 已映射；15 条 UI2D 记录（含 `0x4401` 640×480 复制）；VEV 视图事件 |
| `02-gpu-command-stream.txt` | 42 个命令块摘要；块 22/37 全文与注解；纹理绑定表（`0x0589<<3`） |
| `03-lz77-decodes.txt` | shim 的 `LDEC>` 记录：源 VA、长度、目的 PA、长度；资源布局 |
| `04-guest-hooks.txt` | CPU 图形钩子整场 0 调用、2D submit 15 次、HINFO/HKICK/HDONE |
| `05-gdbstub-watchpoint.txt` | gdbstub 首次命中 libc 拷贝/清零的 PC/寄存器/字节；说明为什么还没抓到命令构建者 |
| `06-maps.txt` | 去重后的关键映射（libObj/viewUnified2/viewSettingMenu） |

## 判据说明

- “调用次数”来自钩子日志标记（`UICLR/UICPY/UIFILL/UIRECT/UI2D`），不是推测；
- 命令块是 QEMU 模型从客户机物理内存读出的原始字，未经改写；
- `0x0589 << 3` 与 `0x0112 << 4` 的地址对照是强证据（见 02/03）；
- gdbstub 的命中只说明“第一次写命令环页的是 libc”，**不**说明构建者是谁；
- 所有否定结论（CPU 路径 0 调用、vpg 崩溃、VA 采样错误）都保留在文档里。

## 如何重新生成

见 `../scripts/boot/README.md` 与 `../scripts/analysis/README.md`。
