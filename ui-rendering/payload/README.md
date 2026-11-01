# 已构建的 guest 插件与 shim

这些是**我们自己的代码**编译出的 ARM32 共享对象，不是 Sony 二进制。它们对应
`../scripts/guest/` 里的源文件，用于复现实验；也可以按
[`../scripts/guest/build-plugins.sh`](../scripts/guest/build-plugins.sh) 重新编译。

| 文件 | 来源 | 用途 | 备注 |
|---|---|---|---|
| `vopen.so` | `scn_openview.c` | 最小探针：打开 `view/BASE_MENU` | 重编译版本 |
| `vui.so` | `scn_ui.c` | CPU 图形钩子 + 2D submit + maps（主用） | **实验时使用的二进制** |
| `vhk2.so` | `scn_hook.c` | 2D submit/kick 钩子 + maps | **实验时使用的二进制** |
| `vmapk.so` | `scn_mapk.c` | 与 `vhk2` 相近的对照版本 | 重编译版本 |
| `vmaps.so` | `scn_maps.c` | 只 dump maps | 重编译版本 |
| `vdefs.so` | `scn_defs.c` | 结构/地址探针 | 重编译版本 |
| `vosd.so` | `scn_osd.c` | OSD 入口探针 | 重编译版本 |
| `vgi.so` | `scn_gi.c` | GraphicsImage 入口探针 | 重编译版本 |
| `vgrm.so` / `vgrm2.so` | `scn_grm.c` / `scn_grm2.c` | GRM 入口探针 | 重编译版本 |
| `vtex.so` | `scn_tex.c` | 纹理 VA 镜像 | **实验时使用的二进制** |
| `vpg.so` | `scn_pg.c` | pagemap 采样（**有风险**，不要长跑） | **实验时使用的二进制** |
| `shim_uipc.so` | `shim_uipc.c` | 预加载 shim（放进镜像 `/etc/shim_io.so`） | **实验时使用的二进制** |
| `shim_io.so` | `shim_io.c` | 早期最小 shim（对照） | 重编译版本 |

## SHA-256

见同目录 `SHA256SUMS`。二进制不是逐字节可复现（clang/lld 版本、BuildID、
时间戳会变）；重编译后哈希不同是正常的，行为以源码为准。标注“实验时使用”的
文件就是产生 `evidence/` 与 `artifacts/` 的那一份。

已验证：用 `scripts/fat/fat12_cat.py` 从实验镜像的 FAT 分区读回 `SHIM_IO.SO`，
SHA-256 为 `0c22aa20414e9ac3eafde1028588e523b555acce529a7802155f23ccea95c3bd`，
与 `shim_uipc.so` 完全一致（写入有读回证据）。

## 构建命令（与 build-plugins.sh 相同）

```sh
clang --target=arm-linux-gnueabi -mfloat-abi=soft -fPIC -shared -nostdlib \
      -fuse-ld=lld -O2 -o vui.so ../scripts/guest/scn_ui.c
```

## 使用

- 插件：通过 `A7M3_SEND_SO=<path>` 加载到 PA `0x10000000`，guest 命令 dd 到
  `/tmp/v.so`，由 `scenario.elf` 调用；
- shim：写入镜像 FAT12 分区的 `/etc/shim_io.so`，并让 `/etc/ld.so.preload`
  指向它（见 `../scripts/fat/README.md`）。

> 注意：guest 命令只 dd 8 KB；`vui.so` 等主插件都小于 8 KB，但 `shim_uipc.so`
> 是预加载库，不受此限制。
