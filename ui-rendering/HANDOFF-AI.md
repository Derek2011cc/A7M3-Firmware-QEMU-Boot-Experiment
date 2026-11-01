# A7M3（ILCE-7M3，4.03 服务模式转储用户态）QEMU 原生菜单/显示 —— AI 交接文档

> 本文件面向接手的 AI/人。目标：在 ma1co/qemu 的 `cxd90045` 机器上跑 A7M3 固件，
> 最终把**相机原生菜单/UI 的像素显示出来**。当前已做到"能跑、能打开菜单视图、能抓 GPU
> 命令流、并能用 QEMU 内的软件解释器把 App 自己的绘制命令合成成 640×480 帧"；尚未做到
> 像素级正确（文字仍不可读，shader/顶点/纹理寻址语义未完全解出）。

> 本文件按用户要求已放入仓库，包含本机（Windows + WSL）路径；公开发布前请按仓库
> AGENTS.md 的要求去掉机器专属绝对路径（脚本已支持用环境变量覆盖默认路径）。

> **2026-11-01 接手进展**：已用 libObj 内的静态命令模板证实坐标变换
> `pixel=(v+W)/2`（W/H 来自 `0x0049`），并实现绘制时刻纹理转储与按矩形缩放采样；
> 纹理格式仍未确定，gdbstub 抓命令构建函数的尝试失败。详见
> [FINDINGS-2026-11-01.md](FINDINGS-2026-11-01.md)。


---

## 0. 当前结论（一句话）

- 相机固件可启动到 App、`viewSettingMenu.uxc` 菜单视图会加载并提交 GPU 绘制；
- 菜单绘制**走 GPU（dmpNative/Sugilite）**，不走 `GraphicsImage::clear/copy`/`GRM_gperm*` 的
  CPU 软件路径（这些钩子整场零调用）；
- 已在 QEMU 的 Sugilite 模型里实现"命令环解释 + 软件光栅"，把 App 自己的绘制命令合成到
  物理表面 `0x202e0200`，已产出 PNG/GIF 帧（`artifacts/`）；
- 剩余核心问题：命令构建函数、屏幕坐标变换、纹理采样/shader、顶点/索引语义；
  解出后即可做到接近原生。

### 最近一次动态追踪（重要）

`uiprobe19` 用 QEMU gdbstub 对命令环 VA 下 `Z2` 写观察点，第一次命中是：

```
stop T05thread:01;watch:0cdca3f0;
PC=04616350 r0=0cdcaa70 r1=136b6838 r2=ffffffe8 r3=ffffd8f1 r12=00000018 lr=02053bb0
code 048080e4 04e080e4 e001bde8 822fb0e1
```

- `0x04616350` 落在 `/lib/libc-2.11.2.so` 的映射 `0x045b6000-0x046a3000`（偏移 `0x60350`）；
- 指令字节 `e4808004` = ARM `str r8,[r0],#4`，是 libc 的 memset/memcpy 类循环；
- 因此这条命中是**驱动在清零命令环**，不是构建函数。构建命令字的代码在这之后；
- 下一步需要"跳过 memset 后再命中构建者"（见 §7）。

---

## 1. 环境与关键文件（本机）

### 1.1 主机

| 项 | 值 |
|---|---|
| 宿主 | Windows 11 + WSL2（Ubuntu） |
| 仓库 | `F:\a7c2-re-main\a7c2-re-main`（报告目录 `report/2026-10-17-qemu-native-camera/`） |
| Agent 临时目录 | `C:\Users\derek\AppData\Local\Temp` |
| WSL home | `/home/derek` |
| QEMU 源码 | `/home/derek/qemu-ma1co`（ma1co/qemu 的 `cxd90045` 机器） |
| QEMU 构建 | `make -C /home/derek/qemu-ma1co/build-arm -j8` → `build-arm/arm-softmmu/qemu-system-arm` |
| 固件目录 | `/home/derek/firmware/a7m3/` |
| 内核 | `vmlinux-console.bin`（4,117,760 B） |
| initrd | `initrd-qemu.img`（2,178,048 B） |
| eMMC 镜像 | `qemu-emmc-2g.sen.img`（2,148,007,936 B；`snapshot=on`，所有写入不落盘） |

### 1.2 镜像内的 shim / FAT 分区

- eMMC 镜像在文件偏移 `0xb780000` 有一个 FAT12 分区；参数：
  `SS=512, SPC=32, RESV=1, NFATS=1, ROOTENTS=512, FATSZ=3, TOT=24576`。
- 其中 `/etc/shim_io.so`（39,228 B，即 `/home/derek/uipc/shim_uipc.so`）与
  `/etc/ld.so.preload`（内容 `"/etc/shim_io.so\n"`，16 B）。
- 读 FAT：`scripts/fat/fat12_ls.py`、`fat12_cat.py`；写 FAT：`scripts/fat/fat12_add2.py`。
- 当前所有运行都用 `qemu-emmc-2g.sen.img`（内含 shim）。修改 shim 可用
  `scripts/fat/fat12_add2.py` 在镜像副本上注入，再用 `fat12_cat.py` 读回校验
  （见 `scripts/fat/README.md`）。

### 1.3 从明文分区提取的固件库

- `/home/derek/n15.img`：314,572,800 B，明文 `/usr` ext2 分区（= 运行时 `/usr`）。
- 已提取：
  - `/home/derek/libObj.so`（19,825,508 B；运行时 `/usr/lib/libObj.so`）
  - `/home/derek/a7m3-viewUnified2.so`（10,854,816 B；视图管理器/注册表+uxc 引擎）
  - `/home/derek/a7m3-viewUnified3..8.so`
  - `/home/derek/a7m3-im.elf`（23,240 B）
  - `/home/derek/libmpr-runtime.so`（12,516,284 B；`/usr/lib/libmpr.so`）
  - `/home/derek/ld-2.11.2.so`、`/home/derek/libIMDB.so`
- libc 在 `/rootfs`（不是 `/usr`）：运行时映射为 `/lib/libc-2.11.2.so`，
  在 `uiprobe19` 中映射于 `0x045b6000-0x046a3000`。**尚未提取成文件**（可通过插件读
  `/rootfs/lib/libc-2.11.2.so` 或从 rootfs 分区 dump 得到）。

### 1.4 启动命令（harness）

`scripts/boot/a7m3-shload.py`：

```
QEMU -M cxd90045
  -kernel .../vmlinux-console.bin -initrd .../initrd-qemu.img
  -drive if=mtd,file=<IMG>,format=raw,snapshot=on
  -serial unix:/tmp/a7m3-<TAG>.sock,server=on,wait=off
  -display none -no-reboot
  -device loader,file=/home/derek/avcam403/av-cam.bin,addr=0x035c6000,force-raw=on
  [-device loader,file=<plugin.so>,addr=0x10000000,force-raw=on]   # A7M3_SEND_SO
  [A7M3_QEMU_EXTRA 追加，如 -qmp unix:...,server=on,wait=off 或 -gdb tcp::1234]
```

环境变量：

- `A7M3_LIRO_AUTOREADY=1`：**必需**。让 LIRO 状态寄存器读回全 1，固件才能越过忙等。
- `A7M3_SEND_SO=<plugin.so>`：把插件加载到 PA `0x10000000`；guest 命令再 dd 到 `/tmp/v.so`。
- `A7M3_PUMP=<sec>`：发送命令后继续收串口的秒数（常 100）。
- `A7M3_QEMU_EXTRA`：额外 QEMU 参数（QMP / gdbstub）。
- 插件相关：`A7M3_SUGI_CMDS=1` 打开 Sugilite 命令/寄存器 dump；`A7M3_SUGI_EXEC=1`
  打开软件执行；`A7M3_SUGI_CENTER=1` 居中坐标模式；`A7M3_SURF_DIR=<dir>` 每绘制写帧。

guest 命令文件 `scripts/boot/a7m3-cmds-ui1.txt`：

```
sleep 5; mount -t tmpfs none /tmp; busybox dd if=/dev/mem bs=4096 skip=65536 count=2 of=/tmp/v.so; /usr/bin/scenario.elf ../../tmp/v b:0x01; sleep 60; echo DONE
```

即：把 PA 0x10000000 的 8 KB 插件拷到 `/tmp/v.so`，由 `libtestcmd_scn` 在 **im.elf 进程内**
`dlopen` 并调用 `scenario_run(data, size, arg2)`。

### 1.5 插件 ABI / 关键地址（libObj 基址会 ASLR，用 `dladdr(openView)` 取）

- `viewManagerIf::openView(const char*, int)`：libObj + `0x1349dc`（Thumb），
  **静态调用形式** `ov("view/BASE_MENU", 0)`；内部构造 Event 并 `EventManager::push(...,true)`，
  末尾 `LkmGpxIF::UnsetMute`。
- 已观察到的 libObj 运行基址：`0x0634b000`、`0x063a8000`、`0x064f4000`、`0x0658b000`、
  `0x0660e000`、`0x06808000`（ASLR）。
- 函数偏移（相对 libObj）：
  - `GraphicsImage::clear` 出口 `0x611662`，实现 `0x611590`（首字 `0xb089b5f0`）
  - `GraphicsImage::copy` 10 参实现 `0x6119bc`（首字 `0x4ff0e92d`）
  - `GRM_gpermFillrect` `0x64ea98`、`GRM_gpermRectblit` `0x64ebd2`（首字 `0x4ff0e92d`）
  - rm 2D submit `0x73a100`（首字 `0x41f0e92d`）、kick `0x73a290`（10 B，首字 `0x4604b5b0`）
- 内联钩子做法（插件内）：目标首条指令改 `0xf000f8df` + `hook|1`（Thumb `ldr.w pc,[pc,#0]`），
  原字节复制到 RWX mmap 的 trampoline，尾部绝对跳回；用 `mmap2(192)`、
  `cacheflush(0x0f0002)`、`mprotect(125)`。

---

## 2. App / 菜单流水线（已确认的事实）

1. **菜单视图确实会加载并绘制**：GPU 提交期间多次 dump `/proc/self/maps`，每次都出现
   `/usr/share/app/viewSettingMenu.uxc`；`viewUnified2.so` 的注册表把
   `view/BASE_MENU` 映射到 `viewBaseMenu.uxc`/`viewBaseMenu.so`。
2. **不是 CPU 软件绘制**：`GraphicsImage::clear/copy`、`GRM_gpermFillrect/Rectblit`
   四个正确安装的内联钩子，在整个会话（含菜单打开后）**零调用**。
3. **绘制走 GPU 命令环**：
   - App 通过 `0x1804` 寄存器写命令缓冲区地址（PA，字节地址），`0x1800=4` 触发 kick；
   - 命令字格式：`(opcode<<16)|0xf000` 后跟一个 value；也有 `0x0450f026` 这类带低位
     标志的块（后跟 5 个 float/常量）；
   - 解出的 opcode：
     - `0x0112` 目标表面地址（值 `<<4`；实测 `0x0202e020` → PA `0x202e0200`）
     - `0x0113` 表面尺寸（`0x01e00280` = 640×480）
     - `0x0116` 源/工作表面（值 `<<4`；实测 `0x02053bb0` → `0x2053bb00`）
     - `0x0117=1`、`0x0010=0x12345678`（同步/魔法）
     - `0x0041=2`；`0x0042/0x0043` = float 320.0/240.0
     - `0x0047`/`0x0048` = 4×int16 矩形（低 16=x、高 16=y）
     - `0x0049=640×480`；`0x004a=0`
     - `0x0105=0xff00ff70`（颜色/状态）；`0x010b/0x010c=0xf`
     - `0x0580=9 ... 0x0580=1` 纹理块；`0x0586/0x0587` 纹理尺寸（高 16=W，低 16=H，
       如 `0x028001e0`→640×480，`0x00400038`→64×56）；`0x0589` 纹理地址（值 `<<3`）；
       `0x058f=4`
     - `0x0202` 绘制（低位的 4 / 0x7c = 顶点数）；`0x0203` **索引缓冲**地址
     - `0x0202` 后紧跟的 count/address 对指向**嵌套命令列表**（`0x0458/0x0459`
       属性写入、`0x0460/0x0461` 浮点常量、`0x0450..0x0454` uniform）
     - `0x0400..0x0410`、`0x0450f026` 块、`0x0460/0x0461` 成对常量（shader uniform）
4. **地址编码验证**（强证据）：
   - `0x0445c040 << 3 = 0x222e0200`：正是 LZ77 解码出的 640×480 背景
   - `0x044827c0 << 3 = 0x22413e00`：正是 640×72 条带
   - 其余 0589 值全部落在 `0x2275xxxx–0x228fxxxx` 的 UI 资源区
5. **LZ77 资源解码**（shim 日志 `LDEC> ... b=` 的 8 个字）：
   - `m[0]=src VA, m[1]=srclen, m[2]=dst PA, m[3]=dstlen, m[4]=2, m[5]=3, m[6]=0, m[7]=0x90`
   - 已见目的地址：
     - `0x222e0200`，614,400 B（640×480 背景，内容为均匀 `0x000f`）
     - `0x2238cb00..0x22413e00`，5×92,160 B（640×72 等条带图）
     - `0x226e0200 + n*0x2000`，8064/1920/16128/3840/2560/1024 B 等（图标/字形/布局块）
   - 还有一部分 UI 纹理不在 LDEC 日志里（App 自己拷贝/生成），例如
     `0x2276c180`(96×72)、`0x22772380`(56×48，橙色砖纹)、`0x22773900`(64×56，
     黑底灰条)、`0x22775580/0x22775800`(16×16)、`0x22772080/0x22772200`(8×16 字形)。
6. **动画证据**：同一元素的 `0x0047/0x0048` 从负 y（如 `0xfe48fff4` → y=-440）逐帧变为
   正 y（如 `0x015c...` → y=348），确认矩形就是屏幕/虚拟坐标。
7. **显示翻转**：`0x0c18` 2D 引擎任务中 flags=`0x4401` 把 640×480 表面从 `0x202e0200`
   复制到 `0x22a80000`/`0x22ac5800`（很可能是显示缓冲翻转）；flags=`8` 是小的线性 DMA 拷贝
   （c00/c04 是地址 `>>2`，c20 字节数，c24/c28=N）。

---

## 3. 我们已经实现的软件渲染（QEMU 内）

文件：`/home/derek/qemu-ma1co/hw/arm/bionz.c`（补丁见 `qemu/qemu-bionz-softrender.patch`；
原始备份 `qemu/bionz-orig.c`）。构建：`make -C /home/derek/qemu-ma1co/build-arm -j8`。

### 3.1 命令解释器

- `cxd90045_sugi_capture_cmd()` 在 `0x1800` kick 时被调用（需 `A7M3_SUGI_CMDS=1`）；
- 读取 `0x1804` 指向的命令缓冲区（最多 1024 word），维护：
  `sugi_soft.surf/tex/texw/texh/r47/r48/color`；
- 遇到 `0x0202` 调 `cxd90045_sugi_soft_draw()`；
- `0x0c18` kick 时（需 `A7M3_SUGI_EXEC=1`）执行 flags=8 的线性 DMA 拷贝
  （`src=c00<<2, dst=c04<<2, n=c20`）。

### 3.2 软件光栅

- 目标固定按 640×480、RGBA4444（A 在低 4 位）处理，表面地址来自 `0x0112<<4`；
- 纹理源地址来自 `0x0589<<3`，尺寸来自 `0x0586/0x0587`（W=高16，H=低16）；
- 透明（A=0）跳过，半透明按 4bit 通道 source-over 混合；
- 三种坐标模式（环境变量选择）：
  - **默认（直投）**：矩形直接用；纹理缩放到矩形；产出 `artifacts/surf*-rgba4444.png`
  - `A7M3_SUGI_HALF=1`：坐标 `>>1`；内容缩在左上 1/4，**已否定**
  - `A7M3_SUGI_CENTER=1`：`screen=(virtual+640/480)/2`，纹理 1:1 采样并 wrap；
    产出 `artifacts/c60-rgba4444.png`，结构上最像菜单（顶部图标行、中部橙色纹理带、底部面板）
    —— 但这只是当前最像的一张，**坐标模式仍未证实**
- 帧导出：`A7M3_SURF_DIR=<dir>` 时每次实际绘制后写 `frame_XXXX.raw`（640×480 RGBA4444，
  614,400 B）。`scripts/analysis/gpu/frames2gif.py` 转 PNG/contact sheet/GIF。

### 3.3 已知效果

- 直投模式：橙色纹理带在顶部、右下字形、底部面板/条纹；
- 居中模式：顶部一行小图标、中部橙色砖纹带、下方点线/字形/面板；
- 文字仍不可读；部分纹理（96×72、296×48）内容稀疏/像噪声，疑似 shader 使用的遮罩或
  未填充的 render target。

---

## 4. 关键实验与日志

所有串口/QEMU 日志在 `F:\a7c2-re-main\a7m3-qemu-logs\`（`.log` 是 guest 串口，`.err` 是
QEMU stderr）。重要的：

| 日志 | 内容 |
|---|---|
| `a7m3-uiprobe2.log/.err` | 首次确认 `viewSettingMenu.uxc` 在绘制期间加载；48 H2D+48 kick |
| `a7m3-uiprobe3.err` | 完整 256-word 命令环 dump（42 条），解出 opcode |
| `a7m3-uiprobe4.err` | 纹理/表面 dump；`SUGI-TEX/DST/SRC` |
| `a7m3-uiprobe5/6` | 纹理 VA 读取、pagemap（不可用）、后期采样 |
| `a7m3-uiprobe7/8/9/10/11` | QMP 大块 pmemsave（0x202e0200、0x222e0200、0x226e0000+、
  0x22770000、0x228f0000、0x2055f000、0x20530000）；软件渲染结果 |
| `a7m3-uiprobe13` | 首批逐绘制帧（`/tmp/a7m3-frames`），GIF/联系表 |
| `a7m3-uiprobe16` | 居中坐标模式帧（`/tmp/a7m3-frames-center`），`artifacts/c60-*` |
| `a7m3-uiprobe18` | gdbstub `Z2` 首次命中（libc memset）；HINFO 基址 `0x0660e000` |
| `a7m3-uiprobe19` | maps 全量 dump + 动态 watchpoint：命中 PC `0x04616350` = libc-2.11.2 memset |

QMP 原始 dump 在 WSL `/tmp/a7m3-pm-*.raw`（disp/tiles/mini/tex228f/bg/gpu1/gpu2）；
帧目录只保留了 `/tmp/a7m3-frames-center`（60 帧；直投/半坐标的帧已清理，可按需重跑）。

---

## 5. 工具清单

| 目录/文件 | 用途 |
|---|---|
| `scripts/boot/a7m3-shload.py` | 主 harness：启动 QEMU、串口 socket、发送 guest 命令、可选插件加载 |
| `scripts/boot/a7m3-shellrun.py` | 启动到 shell 并跑任意命令（列目录/拷文件用） |
| `scripts/boot/a7m3-cmds-ui1.txt` | 打开菜单的 guest 命令（dd 插件 + scenario.elf） |
| `scripts/boot/watch_pm.py` | QMP 观察者：等 "SCN done"，延时后 pmemsave 各区域 |
| `scripts/boot/rsp_*.py` | QEMU gdbstub RSP 客户端：`Z2` 写观察点、读寄存器/内存 |
| `scripts/guest/scn_openview.c` | 最小插件：dlsym openView 并打开 BASE_MENU |
| `scripts/guest/scn_hook.c` | 2D submit/kick 内联钩子，日志 H2D/HKICK（另含全量 maps dump） |
| `scripts/guest/scn_ui.c` | GraphicsImage/GRM 钩子 + 2D + maps 观察（vui.so） |
| `scripts/guest/scn_tex.c` | 读纹理 VA 并镜像到扫描面（vtex.so） |
| `scripts/guest/scn_pg.c` | pagemap/VA 采样（vpg.so，**有风险，见 §6**） |
| `scripts/guest/shim_uipc.c` / `shim_io.c` | 预加载 shim：argv 加 target=qemu、ldec 软件 LZ77、UIPC/事件观测 |
| `scripts/guest/uipc-probes/` | 早期 UIPC 消息探针源码（保留作历史/线索） |
| `scripts/analysis/gpu/` | 命令解析、LZ77 解析、纹理/帧渲染（`cmda_*`、`ldec_*`、`frames2gif` 等） |
| `scripts/analysis/libobj/` | libObj 偏移、PLT 调用、字符串/菜单定位（`plt_inv`、`bcall` 等） |
| `scripts/fat/` | FAT12 分区读写/注入（`fat12_*`） |
| `qemu/` | QEMU 改动补丁与对照源码 |
| `payload/` | 已构建的插件/shim（附 SHA-256 和构建命令） |

插件二进制的权威副本现在在 `payload/`；本机工作副本仍是
`/home/derek/uipc/{vui.so, vhk2.so, vpg.so, vtex.so, shim_uipc.so}`。

---

## 6. 注意 / 陷阱（已踩过）

1. **`scn_pg.c`(vpg.so) 有风险**：它先读一次 `/proc/self/maps` 记下可读 VA，之后直接读
   `0x0445c040` 等地址；该映射可能在稍后被卸载，导致 App 在 `THREAD_INFRA0` 发生
   translation fault（`0x0445d000`），整个菜单不再绘制。做帧/长跑时用 `vui.so` 或纯 openView
   插件，不要用 vpg。
2. **插件必须 ≤ 8 KB**（guest 命令只 dd 2 个 4 KB 页）；超过要改 count。
3. PowerShell 会吞 `$var`、`&&`、以及部分引号；命令行尽量写脚本文件，通过 `wsl bash -lc
   "python3 /mnt/c/.../script.py"` 执行。
4. QEMU 构建只有 `none`/`curses` 显示后端，没有 GTK/SDL；"实时窗口"需要重新 configure 并
   安装 GTK/SDL 依赖。当前用 QMP pmemsave 或模型内 `A7M3_SURF_DIR` 逐帧导出。
5. `-gdb tcp::1234` 连接后 VM 会停（T02）；RSP 脚本必须先发 `c` 恢复，否则 harness 等
   串口超时、QEMU 被杀（`W00`）。
6. gdbstub `Z2` 命中后若直接 `c` 会反复命中同一条指令（memset 循环）；要 `z2` 删除观察点
   或单步越过，否则日志会刷屏且 VM 卡住。
7. 相机 `/etc/ld.so.preload` 机制危险；本工作只用 QEMU `snapshot=on` 的镜像副本，不碰真机。
8. 仓库约定：相机上执行的脚本必须纯 ASCII；证据要读回校验；失败/否定结论也要留档。

---

## 7. 下一步（建议顺序）

1. **复现当前结果**（30 分钟）：
   - 构建 QEMU（补丁在 `qemu/`）；
   - 跑 `scripts/boot/a7m3-shload.py`，命令文件 `scripts/boot/a7m3-cmds-ui1.txt`，插件
     `payload/vui.so`；
   - 用 `scripts/boot/watch_pm.py` 或 `A7M3_SURF_DIR` 得到帧，对照 `artifacts/`。
2. **确定屏幕变换/采样**（最高优先级）：
   - 现在三种坐标假设：直投、`>>1`、居中 1280×960；居中版在现有产物里最像菜单，
     但**没有一种被证实**；
   - 推荐直接找 **命令列表构建函数** 来读语义，而不是继续猜：
     - 方法 A：在 QEMU 里对命令环地址做 `Z2`，先命中 libc memset；需要"越过 memset 后
       再重新武装"（例如命中后 `z2` 删除、用 gdbstub 单步 N 条或临时断到 memset 返回，
       再 `Z2` 重新加；或者用 QEMU 侧的条件记录）；
     - 方法 B：给 `0x1804/0x1800` 的 MMIO 写路径加栈回溯（QEMU 在写寄存器时读
       `current_cpu` 的 LR/SP，抓调用链）；
     - 方法 C：在 App 内 hook kick 函数的调用者（0x73a6e2 / 0x73c8a4 所在的提交路径），
       顺着 job 结构找构建者。
   - 目标：拿到 `dmpNative` 绘制调用的参数（纹理、矩形、混合、采样、顶点缓冲）。
3. **解码顶点/常量**：
   - `0x0203` 指向的是 **16-bit 索引缓冲**（如 `0x2055f850`），不是平坦顶点数组；
   - `0x0202` 后的 count/address 对指向嵌套命令列表（如 `0x20556210`、
     `0x2053a4b0`、`0x205564b0`），里面是 `0x0458/0x0459` 属性写入和
     `0x0460/0x0461` 浮点常量；
   - `0x0216`（如 `0x2055fd10`）是常量块（1.0/16.0/428.0 等），可能含视口变换；
   - 先把**一个**小矩形的属性流完整解析出来，再和截图对照。
4. **补 0x4401**：把 640×480 表面复制到 `0x22a80000`/`0x22ac5800`（显示翻转），确认是否
   需要让 App 的显示链路继续。
5. **可选**：重新构建带 GTK/SDL 的 QEMU，把 `0x202e0200` 作为实时窗口显示（A7C2 的
   ramfb/GTK 方案可参考本仓库 `2026-10-17-qemu-native-camera/source/qemu-sony-native.patch`）。
6. **整理入库**：把可复现部分（补丁、工具、文档）按仓库约定脱敏后提交；证据/大文件不进 git。

---

## 8. 已确认的否定结论（避免重复走弯路）

- 菜单绘制**不**经过 `GraphicsImage::clear/copy`、`GRM_gpermFillrect/Rectblit`（零调用）。
- `viewManagerIf::openView` 的直接调用**确实**打开视图并触发 GPU 提交，但画面不是通过上述
  CPU 软件 GPE 产生。
- 纹理地址 `<<3` 的编码正确（与 LZ77 目的地址精确吻合）；不是任意 handle。
- 纹理 VA 直接按物理地址读（`0x0445c040` 等）在早期是 0；后期该 VA 会变化（App 的匿名映射），
  不能拿它当纹理内容。
- vpg.so 的"读 VA 做镜像"方法会因映射失效导致 App 崩溃，不可用于长时间运行。

---

## 9. 本机环境速查（供接手 AI 直接使用）

```text
QEMU 源码/构建 : /home/derek/qemu-ma1co , build-arm/arm-softmmu/qemu-system-arm
固件           : /home/derek/firmware/a7m3/{vmlinux-console.bin,initrd-qemu.img,qemu-emmc-2g.sen.img}
插件/shim      : /home/derek/uipc/*.so
guest 库       : /home/derek/libObj.so, /home/derek/a7m3-viewUnified2.so, /home/derek/n15.img
harness        : <repo>/a7m3/ui-rendering/scripts/boot/a7m3-shload.py
日志           : F:\a7c2-re-main\a7m3-qemu-logs\a7m3-uiprobe*.log/.err
QMP raw        : /tmp/a7m3-pm-*.raw
帧             : /tmp/a7m3-frames-center/frame_*.raw
产物           : <repo>/a7m3/ui-rendering/artifacts/
```

复现一条最小命令（示例，路径按本机）：

```bash
cd /home/derek
A7M3_LIRO_AUTOREADY=1 \
A7M3_SEND_SO=<repo>/a7m3/ui-rendering/payload/vui.so \
A7M3_SUGI_CMDS=1 A7M3_SUGI_EXEC=1 A7M3_SUGI_CENTER=1 \
A7M3_SURF_DIR=/tmp/a7m3-frames-center A7M3_PUMP=100 \
python3 <repo>/a7m3/ui-rendering/scripts/boot/a7m3-shload.py \
  /home/derek/firmware/a7m3/qemu-emmc-2g.sen.img demo \
  <repo>/a7m3/ui-rendering/scripts/boot/a7m3-cmds-ui1.txt 30
```

---

*文档结束。若接手者需要更细的寄存器/命令序列原始 dump，见 §4 日志与 `/tmp/a7m3-pm-*.raw`。*

> **阶段验收（2026-11-01）**：见 [MILESTONE-2026-11-01.md](MILESTONE-2026-11-01.md)。
> 已交付“命令流驱动、几何可核对”的原生菜单结构帧（BASE_MENU / GLOBAL_MENU），
> 坐标变换、命令模板、opcode 表、提交链均已落盘；**文字/像素级还原仍受
> shader/UV/混合/纹理采样限制，未完成**。
