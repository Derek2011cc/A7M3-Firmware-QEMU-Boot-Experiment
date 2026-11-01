# A7M3 固件包（元数据与本地文件）

本目录把 ILCE-7M3（固件 4.03）的逆向结果单独成包：

- **公开部分**（随仓库提交）：分区清单、文件清单、哈希、分析结论、
  `populate.sh` 和 FAT 提取工具。**不含 Sony 二进制。**
- **本地完整部分**（不随仓库提交）：`files/`，由 `populate.sh` 从你自己的
  只读服务模式转储生成，包含 `nflasha*.bin`、内核/initrd、关键模块等。
  完整包也可以放在仓库外（例如在仓库根之外新建 `a7m3-firmware/`），
  避免误提交。

为什么分开：Sony 固件和分区内容是 Sony 的版权材料，公开仓库不应分发；
本仓库的约定也是“只提供源码与复现工具，不附 Sony 原始固件、私有配置、
运行日志或生成程序”。

## 目录

```
firmware/
├── README.md            本文件
├── FINDINGS.md          已探明内容（分区、引导参数、内核、模块、GPU）
├── SUMMARY.md           2026-10-31 实验记录（历史，文件头有修正说明）
├── MANIFEST.md          分区大小/哈希/识别
├── PARTITIONS.tsv       机器可读分区表
├── populate.sh          从自己的转储生成 files/
├── tools/fat-extract.py FAT12/16 列表/提取（独立副本）
├── listings/            公开固件分区的逐文件清单（文本）
└── files/               本地生成，git 忽略：
    ├── service-dump/    17 个 nflasha*.bin（可选，1.36 GB）
    ├── extracted/       内核、initrd、init、关键 .ko/.elf、version.txt 等
    ├── patched/         vmlinux-*.bin、initrd-qemu.img
    └── synthesized/     mbr.bin、partitions.tsv
```

`listings/` 中的 `/setting`、`/log` 等分区只保留在本地完整包，不在公开
仓库发布。

## 生成本地文件

前提：已有自己的只读转储，默认位于
`$HOME/firmware/a7m3/service-dump/nflashaN.bin`（可用 `A7M3_DATA` 覆盖）。

```sh
# 只提取内核/initrd/关键文件（约数十 MB）
bash firmware/populate.sh

# 同时复制全部分区转储（约 1.36 GB）
bash firmware/populate.sh --dumps

# 同时复制合成的 2 GiB eMMC 镜像
bash firmware/populate.sh --image

# 指定输出目录（例如仓库外的完整包）
bash firmware/populate.sh --dest /path/to/a7m3-firmware/files --dumps
```

脚本只读源转储，不做任何写入。输出目录已有文件时会覆盖同名文件；
`files/` 已在 `.gitignore` 中排除。

## 完整性

- 分区级 SHA-256 见 [`MANIFEST.md`](MANIFEST.md)；
- 关键文件哈希见 [`FINDINGS.md`](FINDINGS.md) §8；
- `populate.sh` 结束后会在输出目录生成 `MANIFEST.sha256`，可用
  `sha256sum -c MANIFEST.sha256` 复核。

## 重要发现

先读 [`FINDINGS.md`](FINDINGS.md) §3：

- `nflasha2:/kemco.txt`（1,901 字节）是**主内核**的引导参数，内存部分为
  `mem=1G@0`、`mem=256M@0x80000000@1`、`memrsv=704M@0x14000000:uc`、
  `14M@0x80000000:uc`、`242M@0x80e00000:uc` 等；
- `nflasha1/BOOT/KEMCO.TXT`（815 字节）是 **updater/recovery** 的参数，
  含 `mem=1016M@0x80000000@1`、`memrsv=0x3600000@0x20000000:uc`，
  **不是**主内核配置。

QEMU 已按主内核参数建模（DDR1 256 MiB）；早期
`memrsv=0x20000000@0x20000000:device` 只是可启动的简化近似。

## 发布边界

- 不要提交 `files/`、`nflasha*.bin`、`vmlinux*.bin`、`initrd*.img`、
  `qemu-emmc*.img` 或串口日志。
- `FINDINGS.md`、`MANIFEST.md`、`PARTITIONS.tsv`、`listings/` 只含元数据，
  可以公开。
- 如果要把完整包交给别人，请确认你有权分享对应固件；否则只提供
  `populate.sh` 和哈希，让对方从自己的设备转储。
