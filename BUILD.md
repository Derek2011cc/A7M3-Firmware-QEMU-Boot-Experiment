# 构建与复现

本文从干净的 Linux 主机开始，复现“A7M3 内核 + 相机 rootfs 启动到 BusyBox shell”。
所有路径都是相对的或 `$HOME` 下的示例；不要把机器专属绝对路径写进配置。

## 0. 前置条件

- Linux 主机（本实验在 Ubuntu 上完成）；Windows 可用 WSL2。
- 工具：`git`、`make`、`gcc`、`pkg-config`、`python3`、`sfdisk`（util-linux）、
  `e2fsprogs`（`debugfs`/`dumpe2fs`，仅检查用）。
- QEMU 构建依赖（Debian/Ubuntu 常用包）：

  ```sh
  sudo apt install build-essential git pkg-config libglib2.0-dev \
       libpixman-1-dev zlib1g-dev ninja-build python3
  ```

- 你自己的 A7M3 固件数据（见 §2）。**本仓库不提供任何 Sony 二进制。**

## 1. 构建 QEMU

使用 ma1co/qemu 的 `cxd90045` 机器：

```sh
git clone https://github.com/ma1co/qemu.git "$HOME/qemu-ma1co"
cd "$HOME/qemu-ma1co"
git checkout 1909382c0d3533051cead7fde70ca8e84807ee14
git apply /path/to/a7m3/patches/qemu-a7m3.patch
mkdir -p build-arm && cd build-arm
../configure --target-list=arm-softmmu --disable-werror
make -j"$(nproc)"
```

产物：`$HOME/qemu-ma1co/build-arm/arm-softmmu/qemu-system-arm`。
`scripts/run-shell.sh` 默认使用这个路径；可用 `QEMU_BUILD` 覆盖。

补丁包含三部分（见 [patches/README.md](patches/README.md)）：

- `hw/arm/bionz.c`：功能改动（内存、ATAGS、页表、SD）；
- `hw/char/pl011.c`、`target/arm/helper.c`：调试日志，可去掉，不影响启动。

## 2. 准备固件输入

推荐使用**只读服务模式**转储的本机分区镜像：

```
$HOME/firmware/a7m3/service-dump/nflasha1.bin
$HOME/firmware/a7m3/service-dump/nflasha2.bin
...
```

`build-emmc.sh` 需要至少 2、3、7、10、11、12、15、18；其余分区有就写入，
没有就留空（分区仍存在）。

> 版本注意：本目录的补丁偏移基于 **4.03** 转储。4.04 官方更新包里的
> `vmlinux`/`initrd` 可能不同；所有补丁脚本都会校验原始字节，不匹配会
> 拒绝写入，而不是默默打错位置。

从 `nflasha3.bin`（FAT12）取出内核与 initrd：

```sh
cd "$HOME/firmware/a7m3"
python3 /path/to/a7m3/scripts/fat-extract.py service-dump/nflasha3.bin
python3 /path/to/a7m3/scripts/fat-extract.py service-dump/nflasha3.bin \
        VMLINUX.BIN -o vmlinux.bin
python3 /path/to/a7m3/scripts/fat-extract.py service-dump/nflasha3.bin \
        INITRD.IMG -o initrd-4.03.img
```

核对哈希（应与 [docs/ARTIFACTS.md](docs/ARTIFACTS.md) 一致）：

```sh
sha256sum vmlinux.bin initrd-4.03.img
# 13649dc0...  vmlinux.bin
# 07919005...  initrd-4.03.img
```

若哈希不同，说明固件版本/转储方式不同；后续补丁可能不适用。

也可以直接用 `bash firmware/populate.sh` 一次性提取主内核/initrd、
`nflasha1/BOOT` 的第二套启动对、关键模块和元数据（输出到
`firmware/files/extracted/`）。

## 3. 生成内核副本

按顺序执行（每一步都基于上一步的输出）：

```sh
cd "$HOME/firmware/a7m3"
S=/path/to/a7m3/scripts
python3 "$S/patch-pgdpatch.py" vmlinux.bin        vmlinux-pgdpatch.bin
python3 "$S/patch-atagfix.py"  vmlinux-pgdpatch.bin vmlinux-atagfix.bin
python3 "$S/patch-console.py"  vmlinux-atagfix.bin  vmlinux-console.bin
```

每步都会打印被替换的字和 SHA-256。预期校验值见
[docs/ARTIFACTS.md](docs/ARTIFACTS.md)。

三个补丁的作用：

| 补丁 | 改动 | 目的 |
|---|---|---|
| `patch-pgdpatch.py` | `0x60108068`：`b` 跳过页表清零 | 保留 QEMU 预置的 `swapper_pg_dir` |
| `patch-atagfix.py` | `0x60108028`：改调桩；桩在 `0x60495f40` | 恢复被 stext 覆盖的 ATAGS 指针 |
| `patch-console.py` | `0x602988cc`：`str r3` → `str r2` | 绕过 Sony ADC/errind 控制台门控 |

## 4. 生成 initrd 副本

```sh
S=/path/to/a7m3/scripts
python3 "$S/patch-initrd.py" initrd-4.03.img initrd-qemu.img
```

脚本在 ext2 镜像内做两处**等长**替换：

- `Backup_load=0 ` → `Backup_load=1 `（`backup.ko` 走文件加载）；
- `/usr/kmod/udate.ko` → `/usr/kmod/udate.zz`（跳过会 NULL 解引用的模块）。

脚本会断言每个字符串恰好出现一次，并在写后读回。输出 SHA-256 应为
`ec4dc5c9...1b4eb8`（见 ARTIFACTS.md）。

> initrd 是 ext2 镜像（不是 cpio）。直接改字节是安全的，因为 ext2 没有
> 校验和，且两处替换都不改变长度。若用其它工具重新打包，不保证可用。

## 5. 生成合成 eMMC 镜像

```sh
cd "$HOME/firmware/a7m3"
bash /path/to/a7m3/scripts/build-emmc.sh "$HOME/firmware/a7m3"
```

脚本会：

1. 建一个 2 GiB 的 user-area 镜像，用 `sfdisk` 写入 MBR（1–25 分区）；
2. 按 `sfdisk` 生成的起始 LBA（大小取自 `MANIFEST.md`）把
   `service-dump/nflashaN.bin` 稀疏写入；
3. 加上 `0x80000` 的 boot 区前缀，生成 `qemu-emmc-2g.img`；
4. 校验 MBR 前 4 行并打印；
5. 调用 `patch-grmgles.py` 打 GPU 驱动桩。

预期产物：`2,148,007,936` 字节（`0x80000` + 2 GiB）。

若脚本报 `image ... > partition ...`，说明某个 `nflashaN.bin` 比 MBR 分区大：
检查 `MANIFEST.md` 或转储是否截断。若 `dd` 之后镜像变小，检查是否漏了
`conv=sparse,notrunc`。

## 6. 启动

```sh
cd "$HOME/firmware/a7m3"
bash /path/to/a7m3/scripts/run-shell.sh
```

等价命令：

```sh
"$HOME/qemu-ma1co/build-arm/arm-softmmu/qemu-system-arm" -M cxd90045 \
  -kernel vmlinux-console.bin \
  -initrd initrd-qemu.img \
  -drive if=mtd,file=qemu-emmc-2g.img,format=raw \
  -serial stdio -display none
```

> 当前 QEMU 已按**主内核**参数建模：DDR0 1 GiB + DDR1 256 MiB，
> `memrsv=704M@0x14000000:uc`、`14M@0x80000000:uc`、
> `242M@0x80e00000:uc` 等。这些参数来自 `nflasha2:/kemco.txt`，
> 不是 `nflasha1/BOOT/KEMCO.TXT`（后者属于 updater）。详见
> [firmware/FINDINGS.md](firmware/FINDINGS.md) §3.0。

期待的顺序：

1. 内核日志；出现 `Memory: 1024MB 256MB = 1280MB total`；
2. `dma330` 附近可能停顿约 10 秒；
3. init 挂载分区、加载模块；
4. 未打 DFB 绕过补丁时，Appframework 启动失败并出现：

   ```
   Exception>
   ```

   （打了 `scripts/experimental/patch-libmpr-dfb16.py` 的副本后，
   `im.elf` 可运行到 63 线程，可能不再出现该提示。）
5. 在该提示符输入 `exit`（`shell` 也可以），打印 `Good Bye.` 后进入：

   ```
   / #
   ```

6. 验证：

   ```sh
   cat /proc/version
   cat /proc/partitions        # nflasha1..25, nflashaB0/B1
   cat /proc/mounts
   ls /usr/bin
   ```

7. 退出 QEMU：`Ctrl-A` 然后 `X`。

## 7. 判据

“启动成功”必须同时满足：

- 串口有内核日志（不是只看 QEMU 退出码）；
- `Memory: 1024MB 256MB = 1280MB total`；
- `/proc/partitions` 出现 `nflasha1..25`；
- `/proc/mounts` 中 `/usr`、`/system`、`/setting` 已挂载；
- 若出现 `Exception>`，输入 `exit` 能到 `/ #` 并执行命令。

只看到内核日志而没有 `/ #`，通常是模块失败；用 `initcall_debug` 日志最后
几行定位。默认 `build-emmc.sh` 会把 `grm_gles.ko` 整体打桩，所以
`/dev/dmpgles2` 不存在、Appframework 在 libc abort——这是默认镜像的预期行为。
想继续实验，见 `scripts/experimental/`（时钟绕过 + 节点 + DFB 重定向）。

## 8. 常见问题

| 现象 | 可能原因 | 处理 |
|---|---|---|
| 完全没有串口输出 | 用了未打 console 补丁的内核 | 确认 `-kernel vmlinux-console.bin` |
| `setup_arch` Data Abort | pgd 补丁或 ATAGS 修复缺失 | 按 §3 顺序重打 |
| init 被 `sugi_hw.c:221` BUG 杀死 | `grm_gles.ko` 未打桩 | 重跑 `build-emmc.sh`（会调用 `patch-grmgles.py`） |
| `backup.ko` 返回 `-EIO` | initrd 未改 `Backup_load=1` | 重跑 `patch-initrd.py` |
| `upm` NULL 解引用 | `memrsv` 缺失/内存模型不对 | 确认 QEMU 补丁与 cmdline 含主内核 `memrsv`（`704M@0x14000000:uc`、`14M@0x80000000:uc`、`242M@0x80e00000:uc` 等） |
| 镜像不是 2,148,007,936 字节 | `dd` 截断 | 必须 `conv=sparse,notrunc` |
| `/dev/dmpgles2` 不存在 | 默认 GPU 桩不注册设备 | 预期；继续实验用 `experimental/` 脚本 |
| `patch-*.py` 报 unexpected bytes | 固件版本/布局不同 | 不要强改；重新核对转储版本 |

## 9. 自检（提交前）

```sh
bash -n scripts/*.sh
python3 -m py_compile scripts/*.py scripts/experimental/*.py
git apply --check -R patches/qemu-a7m3.patch   # 在打过补丁的 QEMU 树中
```

脚本默认路径均为 `$HOME/firmware/a7m3`，可通过第一个参数覆盖；
不要在文档或脚本里写死个人机器路径。

## 10. 发布内容边界

不要提交：

- `service-dump/`、`nflasha*.bin`、`vmlinux*.bin`、`initrd*.img`；
- `qemu-emmc-*.img`（2 GiB 生成物）；
- 串口日志、`/tmp` 证据文件；
- QEMU `build-arm/` 目录。

本目录的 `.gitignore` 已覆盖这些模式。
