# 产物清单与哈希

本页记录实验用到的所有生成物、生成命令和 SHA-256，便于复现和交叉核对。
**Sony 固件、分区转储、内核/initrd 副本、eMMC 镜像和运行日志都不随仓库
分发**；这里只给摘要。

## 1. 输入（不随仓库分发）

| 名称 | 大小（字节） | SHA-256 | 来源 |
|---|---:|---|---|
| `vmlinux.bin` | 4,117,760 | `13649dc02443d761df58ba2745171acb089f7865c12fd384df1191e2c45c5cd6` | `nflasha3` FAT 中的 `VMLINUX.BIN` |
| `initrd-4.03.img` | 2,178,048 | `0791900525b4e5040edd0b233fb513bc242a3480eca88feac5c355410def9d10` | `nflasha3` FAT 中的 `INITRD.IMG` |

提取：

```sh
python3 scripts/fat-extract.py nflasha3.bin VMLINUX.BIN -o vmlinux.bin
python3 scripts/fat-extract.py nflasha3.bin INITRD.IMG  -o initrd-4.03.img
```

`nflasha3.bin` 是 FAT12（61510 扇区、32 扇区/簇、512 根目录项）。
上述哈希对应 **4.03** 服务模式转储；4.04 官方更新包中的对应文件不一定相同。

## 2. 内核副本（不随仓库分发）

| 名称 | 大小（字节） | SHA-256 | 生成命令 |
|---|---:|---|---|
| `vmlinux-pgdpatch.bin` | 4,117,760 | `2c15f5614defa88d984d94577520828cfb08b8e65b46d720dba4c267f4db20c3` | `patch-pgdpatch.py vmlinux.bin vmlinux-pgdpatch.bin` |
| `vmlinux-atagfix.bin` | 4,117,760 | `054664a1697f9eee1cc9871dd1675f516f21597fd90c2f0d1ffbf126584f289e` | `patch-atagfix.py vmlinux-pgdpatch.bin vmlinux-atagfix.bin` |
| `vmlinux-console.bin` | 4,117,760 | `e6c17a94b813111d3a70b95f303d60c07b40da399146a4c0038d075976169b0f` | `patch-console.py vmlinux-atagfix.bin vmlinux-console.bin` |
| `vmlinux-noncache.bin` | 4,117,760 | `132aa906d826d9dff5a3d64b727a17fa5bacb61db1ed812a8faa9f8b5ab299e3` | 实验（失败）：`experimental/patch-noncache-1g.py` |
| `vmlinux-window.bin` | 4,117,760 | `abcbceb15db462a0f8d55868c8e015e19a7a0ac9b80239ea94e002ce54c80802` | 实验（失败）：`experimental/patch-window-512m.py` |

改动内容（每个文件只差 1–5 个字）：

| 文件 | VA | 原值 → 新值 |
|---|---|---|
| `vmlinux-pgdpatch.bin` | `0x60108068` | `0xE4803004` → `0xEA000004` |
| `vmlinux-atagfix.bin` | `0x60108028` | `0xEB00003A`（`bl __vet_atags`）→ `bl 0x60495f40` |
| 同上 | `0x60495f40` | 16 字节零 → 桩（`ldr r2`/`b`/字面量 `0x00010000`） |
| `vmlinux-console.bin` | `0x602988cc` | `0xE58630D4` → `0xE58620D4` |
| `vmlinux-noncache.bin` | `0x6011f370` | `0xE1510002` → `0xE1520001` |
| `vmlinux-window.bin` | `0x6049cb78` | `0xA2833206` → `0xA3A0320A` |

`vmlinux-noncache.bin` / `vmlinux-window.bin` 是失败实验的历史产物，
仅用于说明问题，**不要用来启动**。

## 3. initrd 副本（不随仓库分发）

| 名称 | 大小（字节） | SHA-256 | 说明 |
|---|---:|---|---|
| `initrd-backup1.img` | 2,178,048 | `7fe50152401e96fd738be76d0af4ba71ff977aa1b0f43dc9f7397adf6302487c` | 只改 `Backup_load=1`（历史中间产物） |
| `initrd-backup1-skipudate.img` | 2,178,048 | `ec4dc5c97219a282d1d93087ade79d1de962d3c1d6bdde0e715510a2b31b4eb8` | 再跳过 `udate.ko` |
| `initrd-qemu.img` | 2,178,048 | `ec4dc5c97219a282d1d93087ade79d1de962d3c1d6bdde0e715510a2b31b4eb8` | 当前脚本的输出，字节与上一行相同 |

生成：

```sh
python3 scripts/patch-initrd.py initrd-4.03.img initrd-qemu.img
```

两处改动（等长、ext2 无校验和）：

| 偏移 | 原文 → 新文 |
|---|---|
| `2,016,076` | `Backup_load=0 ` → `Backup_load=1 ` |
| `2,016,172` | `/usr/kmod/udate.ko` → `/usr/kmod/udate.zz` |

## 4. eMMC 镜像（不随仓库分发）

| 名称 | 大小（字节） | 说明 |
|---|---:|---|
| `qemu-emmc-2g.img` | 2,148,007,936 | `0x80000` boot 前缀 + 2 GiB user area |

生成：

```sh
bash scripts/build-emmc.sh
```

脚本写入 MBR 与分区内容后，把 `grm_gles.ko` 的 `init_module` 改为桩：

| 项目 | 值 |
|---|---|
| 分区 | `nflasha15`（LBA 由 MBR 解析，通常 `1531904`） |
| ext2 块 | `173903`（1024 字节块） |
| 块内偏移 | `324`（文件偏移 `0x1944`） |
| 原 8 字节 | `0dc0a0e1f0d82de9` |
| 新 8 字节 | `0000a0e31eff2fe1` |

`qemu-emmc-2g.img` 有 2 GiB，不适合计算/记录整镜像哈希；可用
`patch-grmgles.py` 的读回输出和 `/proc/partitions` 作为判据。

## 5. QEMU 补丁

| 文件 | 大小 | SHA-256 | 说明 |
|---|---:|---|---|
| `patches/qemu-a7m3.patch` | 11,991 | `9f28a7c0eda1e57833a9565c68355af48b5c9623023412738a403f751b7f64ba` | 相对 ma1co/qemu `1909382c0d3533051cead7fde70ca8e84807ee14` 的差分（3 文件，+175/−6；含 DDR1 256 MiB 与主内核 `memrsv`） |

应用后用 `git diff --stat` 核对；`hw/char/pl011.c` 与 `target/arm/helper.c`
只是调试日志。

## 6. 固件包（`firmware/`）

公开部分（随仓库）：

- `firmware/FINDINGS.md`：分区地图、真实引导参数、内核/initrd、模块、未解项；
- `firmware/MANIFEST.md`：17 个分区/启动分区转储的大小与 SHA-256；
- `firmware/PARTITIONS.tsv`：机器可读分区表；
- `firmware/listings/`：公开固件分区的逐文件清单（文本）；
- `firmware/populate.sh` + `firmware/tools/fat-extract.py`：本地重建工具。

本机完整包（不随仓库）：

- 仓库外的 `a7m3-firmware/`（或 `populate.sh --dest` 指定的目录）；
- 包含 17 个 `nflasha*.bin`、内核/initrd、关键 `.ko`/`.elf`、补丁副本、
  `KEMCO.TXT`、`LOADADDR.TXT`、`nflasha2:/kemco.txt`、`MANIFEST.sha256`。

`nflasha1/BOOT` 的第二套启动对（**updater/recovery**）：

| 文件 | 大小 (B) | SHA-256 |
|---|---:|---|
| `VMLINUX.BIN`（zImage） | 1,697,104 | `b5c68ff96cdac4b5e86027d63698dbd3aa9340a86cb06e13d96f549cdffd92e8` |
| `INITRD.IMG`（cramfs，562 文件） | 4,354,048 | `e92f3344ce92c83d3c9592121cbcf6c43fdf28b0c659f84ee3c2a13cc9157804` |
| `KEMCO.TXT` | 815 | `0f9c5cd243da4ebbb412bc1d34d3752a6e7d875ed5db6f4034d53029c3f5a4e7` |
| `LOADADDR.TXT` | 33 | `a55bb7c1ef670cad21301679f75412bf2f81bfe713c3a83acd4b2b457523200d` |

上述 `KEMCO.TXT` 只属于 **updater/recovery**。主内核的参数在另一份文件：

| 文件 | 大小 (B) | SHA-256 |
|---|---:|---|
| `nflasha2:/kemco.txt` | 1,901 | `83b32a57278ee640e27fb60cbeec28a9c74b6055f5fe1b2b728d1c053b5988c8` |

主内核内存部分为 `mem=1G@0`、`mem=256M@0x80000000@1`、
`memrsv=704M@0x14000000:uc`、`14M@0x80000000:uc`、
`242M@0x80e00000:uc` 等；当前 QEMU 已按这份地图建模。详见
[`../firmware/FINDINGS.md`](../firmware/FINDINGS.md) §3.0 和
[`LIRO-AND-MEMORY.md`](LIRO-AND-MEMORY.md) §1。

## 7. UI 渲染实验的生成物（`ui-rendering/`）

- `ui-rendering/payload/*.so`：我们自己的 guest 插件/shim（非 Sony 二进制），
  逐个 SHA-256 见 [`../ui-rendering/payload/SHA256SUMS`](../ui-rendering/payload/SHA256SUMS)；
- `ui-rendering/artifacts/*.png|gif`：软件重放的产物（不是真机截图，像素不正确），
  说明见 [`../ui-rendering/artifacts/README.md`](../ui-rendering/artifacts/README.md)；
- `ui-rendering/qemu/`：Sugilite 软件解释补丁，SHA-256 见该目录 `README.md`；
- 原始串口/QEMU 日志和 QMP `.raw` 体积大，不随仓库分发。

### 6.1 2026-10-31 实验脚本

| 脚本 | 作用 | 结果 |
|---|---|---|
| `scripts/experimental/patch-grmgles-clock.py` | 恢复原始 `init_module`，把 `sugilite_clock_up_internal` 的时钟检查改为返回成功 | `grm_gles` init 返回 0，注册 `dmpgles2` cdev |
| `scripts/experimental/add-dmpgles2-node.py` | 在 nflasha7 静态 `/dev` 中加入 `dmpgles2 c 248 29` | Appframework 可打开并 mmap 128 KiB |
| `scripts/experimental/patch-libmpr-dfb16.py` | 把 libmpr.so 的 DFB 打开路径从 `/dev/nflasha4` 改到 `/dev/nflasha16` | 不再触发互斥量断言；`im.elf` 63 线程 |
| `scripts/experimental/patch-liro-nowait.py` | 把 LIRO `0x63a1d298` 的 `beq` 改成 NOP | 负实验：`entry_main` 返回，但系统仍静默 |

补丁输入/输出（本地副本，不随仓库）：

| 项目 | 值 |
|---|---|
| libmpr 输入 SHA-256 | `ab1df757108d123f403188e33b732bf0c8cded33ff29711417dca14c5ab883a5` |
| libmpr DFB16 输出 SHA-256 | `0cf5513b32520bbade15c11eeb934584c32fe1e604b792b8664bca9b24a4138c` |
| av-cam.bin 输入 SHA-256 | `e1b0f84e8323005a34b43de6dd0083fca712d2c654f0d9716512e0a54631bbe7` |
| av-cam nowait1 输出 SHA-256 | `d301162b3e0564410b9d679be8dc5ee3506c3ee0cbbc168303ef98204f3578f3` |

失败的 `libmpr.so` 单点 `bx lr` 补丁没有保留（ASLR 下调用点变化，行为未改变）。

### 6.2 本地实验镜像（不随仓库）

| 名称 | 说明 |
|---|---|
| `qemu-emmc-2g.dfb16.img` | 在 `qemu-emmc-2g.golden.img` 上套用 libmpr DFB16 补丁；用于 63 线程测试 |
| `av-cam-nowait1.bin` | 套用 LIRO 单点跳过补丁；负实验，不用于发布 |

这些文件只存在于本机实验目录，`.gitignore` 不覆盖时也不要提交。

## 7. 不随仓库分发的东西

- `service-dump/nflasha*.bin`（Sony 固件数据，且体积大）；
- `vmlinux*.bin`、`initrd*.img`；
- `qemu-emmc-*.img`（含 `.golden`、`.dfb16` 等实验镜像）；
- `libmpr*.so` 及其 DFB16 补丁副本；
- `av-cam*.bin` 及其 nowait 补丁副本；
- 串口日志（`*.log`、`*.out`）、`/tmp` 下的证据文件；
- QEMU 的 `build-arm/`。

`.gitignore` 已排除这些模式。若要公开分享实验数据，请只分享你自己生成的
摘要、补丁和脚本，不要上传 Sony 二进制。
