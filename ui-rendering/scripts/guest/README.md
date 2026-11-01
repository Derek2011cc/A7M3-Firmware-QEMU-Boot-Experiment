# guest 侧插件与 shim

这些 C 文件编译成 **ARM32 共享对象**，在 `im.elf` 进程内运行。它们不链接 libc，
只使用原始 `svc` 系统调用；这是为了兼容 guest 的 glibc 2.11.2，并保持插件足够小。

## 1. 构建

```sh
bash build-plugins.sh [输出目录]
```

默认输出到 `../../build/`（避免覆盖仓库里带 SHA-256 的 `payload/`）。核心命令：

```sh
clang --target=arm-linux-gnueabi -mfloat-abi=soft -fPIC -shared -nostdlib \
      -fuse-ld=lld -O2 -o vui.so scn_ui.c
```

- 需要 clang 的 ARM 后端和 `lld`；
- `-nostdlib` 必须保留：插件不依赖 guest 的 libc；
- 可加 `-s` 减小体积（可选，不改变行为）；
- 编译后可用 `file vui.so` 确认 `ELF 32-bit LSB shared object, ARM, EABI5`。

## 2. 插件必须 ≤ 8 KB

guest 命令用 `busybox dd ... count=2` 把 PA `0x10000000` 的 8 KB 拷到 `/tmp/v.so`。
超过 8 KB 的插件会被截断。需要更大时，改 `a7m3-cmds-ui1.txt` 里的 `count`，并确认
`scenario.elf` 的加载逻辑接受更大的文件。

## 3. ABI

`libtestcmd_scn`（`/usr/bin/scenario.elf`）在 `im.elf` 进程内 `dlopen("/tmp/v.so")`，
然后调用：

```c
int scenario_run(unsigned int data, unsigned int size, unsigned int arg2);
```

插件应导出这个符号。返回值被忽略。

## 4. 插件一览

| 源文件 | 构建名 | 用途 | 关键日志标记 |
|---|---|---|---|
| `scn_openview.c` | `vopen.so` | 最小探针：取 libObj 基址并 `openView("view/BASE_MENU", 0)` | `SCN enter`、`SCN base=`、`SCN r1=` |
| `scn_ui.c` | `vui.so` | CPU 图形钩子（clear/copy/fill/rect）+ 2D submit + maps | `UIINFO`、`UIINST`、`UIDONE`、`UI2D`、`UICLR/UICPY/UIFILL/UIRECT` |
| `scn_hook.c` | `vhk2.so` | 2D submit 与 kick 内联钩子，dump 参数和全量 maps | `HINFO`、`HINST`、`H2D`、`HKICK`、`HDONE`、`MAPSB/MAPSE` |
| `scn_mapk.c` | `vmapk.so` | 同 `scn_hook.c` 的另一个版本（保留作对照） | 同上 |
| `scn_maps.c` | `vmaps.so` | 只 dump `/proc/self/maps` | `MAPSB/MAPSE` |
| `scn_defs.c` | `vdefs.so` | 读取 libObj 关键地址/结构的探针 | `SCN`/`DEF` |
| `scn_osd.c` | `vosd.so` | OSD/覆盖层入口探针 | `OSD` |
| `scn_gi.c` | `vgi.so` | GraphicsImage 相关入口探针 | `GI` |
| `scn_grm.c` / `scn_grm2.c` | `vgrm.so` / `vgrm2.so` | GRM/Sugilite 用户态入口探针 | `GRM` |
| `scn_tex.c` | `vtex.so` | 在 App 内读纹理 VA 并镜像到可扫描面 | `TEX` |
| `scn_pg.c` | `vpg.so` | pagemap + VA 采样（**有风险**，见下） | `PG` |
| `shim_uipc.c` | `shim_uipc.so` | 预加载 shim：argv 加 `target=qemu`、`/dev/ldec` 软件 LZ77、UIPC/openView 观测 | `[U t...]` 记录 |
| `shim_io.c` | `shim_io.so` | 早期/最小预加载 shim（保留对照） | `[U t...]` |
| `uipc-probes/*.c` | 多个 | 早期 UIPC 消息探针（历史线索，不参与主流程） | 各文件自定义 |

### `scn_pg.c` 的风险

`vpg.so` 先记录一次可读 VA，之后直接按 VA 读取纹理；该映射可能在稍后被卸载，导致
App 在 `THREAD_INFRA0` 发生 translation fault（曾见 `0x0445d000`），菜单不再绘制。
做长时间运行/出帧时用 `vui.so` 或 `vopen.so`，不要用 `vpg.so`。

## 5. 预加载 shim 与镜像注入

`shim_uipc.so` 放在镜像 FAT12 分区的 `/etc/shim_io.so`，`/etc/ld.so.preload` 内容为：

```text
/etc/shim_io.so
```

`scripts/fat/fat12_add2.py` 可以把文件写进镜像的 FAT12 分区。**只对镜像副本操作**，
不要把真实相机分区挂载为可写。

## 6. 约定

- 源文件必须纯 ASCII（相机侧 shell/日志对非 ASCII 不友好）；
- 不写死 libObj 基址（ASLR）；用 `dladdr`/`_r_debug` 取；
- 内联钩子必须校验目标首字，不匹配就不要 patch；
- 钩子里不要调用可能睡眠的 libc 函数；只用 `write(2, ...)` 记录。
