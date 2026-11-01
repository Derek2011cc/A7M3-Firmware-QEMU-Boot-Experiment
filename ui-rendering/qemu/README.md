# QEMU Sugilite 软件解释补丁

## 1. 内容

| 文件 | 说明 |
|---|---|
| `qemu-bionz-softrender.patch` | 相对 ma1co/qemu 的 `hw/arm/bionz.c` 的差分 |
| `bionz-orig.c` | 打补丁前的 `hw/arm/bionz.c`（对照，SHA-256 见下） |
| `bionz-patched.c` | 打补丁后的完整文件（便于直接替换） |

SHA-256：

```text
534800a96b4d23defedd69c1a08bed8b6a0064d4eac0a0b4d15dbdce38fe1067  bionz-orig.c
8f56e3c41b17b078a6edb6c02147d4a5780f19634cca5cdaa0ce2c83fb372b6b  bionz-patched.c
c19d8f4e425e85a884504584eb36e12d0473b77553004058946a3feb59322cc1  qemu-bionz-softrender.patch
```

`bionz-orig.c` 对应本机 `/home/derek/qemu-ma1co/hw/arm/bionz.c` 的基线；
不同 ma1co/qemu revision 可能不能直接 `git apply`，此时用 `bionz-patched.c`
覆盖或手工合并。

## 2. 应用与构建

```sh
cd /path/to/qemu-ma1co
git apply /path/to/ui-rendering/qemu/qemu-bionz-softrender.patch
make -C build-arm -j8
```

或：

```sh
cp /path/to/ui-rendering/qemu/bionz-patched.c /path/to/qemu-ma1co/hw/arm/bionz.c
make -C /path/to/qemu-ma1co/build-arm -j8
```

## 3. 补丁做了什么

1. **命令抓取**：在 `0x1800` kick 时，读取 `0x1804` 指向的命令缓冲区，向 stderr
   输出 `SUGI-CMDA[n] addr=...: <words>`；同时扫描命令里的指针：
   - `SUGI-TEX`：`0x0589` 的 value（注意：这里打印的是 value 本身，不是 `<<3` 后的 PA）；
   - `SUGI-DST`：`0x0112 << 4`；
   - `SUGI-SRC`：`0x0116 << 4`；
   - `SUGI-PTR`：命令缓冲里落在 `0x20000000-0x30000000` 的字。
2. **软件解释**（`A7M3_SUGI_EXEC=1`）：顺序扫描命令，维护
   `surf/tex/texw/texh/r47/r48/color`，遇到 `0x0202` 调软件光栅。
3. **软件光栅**：640×480 RGBA4444，A 在低 4 位，source-over；三种坐标模式
   （默认直投、`A7M3_SUGI_HALF=1`、`A7M3_SUGI_CENTER=1`）。
4. **帧导出**：`A7M3_SURF_DIR=<dir>` 时每次绘制后写 `frame_XXXX.raw`。
5. **2D 引擎**：`0x0c18` kick 时执行 flags=8 的线性 DMA（`src=c00<<2`,
   `dst=c04<<2`, `n=c20`）。

## 4. 限制

- 软件光栅只使用 `0x0589` 的纹理整图，**没有**读取 `0x0202` 后的嵌套命令列表
  （`0x0458/0x0459/0x0460/0x0461`），也没有解码真实顶点/UV；因此文字/图标不对。
- 命令抓取窗口在产生本仓库日志的构建里是 256 字/块（更早的实现）；当前源码的上限是
  1024 字。需要完整命令包时重新编译并重跑。
- 纹理格式/tiling 未定；当前按线性 RGBA4444 采样。
- 不模拟真实 GPU 的定时/中断/显示输出；只做命令解释。

## 5. 调试环境变量

| 变量 | 作用 |
|---|---|
| `A7M3_SUGI_CMDS=1` | 打开 `SUGI-*` dump |
| `A7M3_SUGI_EXEC=1` | 打开软件解释/光栅 |
| `A7M3_SUGI_HALF=1` | 半坐标模式 |
| `A7M3_SUGI_CENTER=1` | 居中坐标模式 |
| `A7M3_SURF_DIR=<dir>` | 逐帧导出目录 |

详见上级 [UI-RENDERING.md](../UI-RENDERING.md) §5。
