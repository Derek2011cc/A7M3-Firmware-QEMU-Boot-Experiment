# 启动 harness 与观察工具

这些脚本假设你已经按 [../../../BUILD.md](../../../BUILD.md) 生成了：

- `vmlinux-console.bin` / `initrd-qemu.img`
- `qemu-emmc-2g.sen.img`（内含 `/etc/shim_io.so` 与 `/etc/ld.so.preload`）
- `av-cam.bin`（LIRO 固件）

默认路径是开发机上的路径，可用环境变量覆盖：

| 变量 | 默认值 | 作用 |
|---|---|---|
| `A7M3_DIR` | `/home/derek/firmware/a7m3` | 内核/initrd 目录 |
| `A7M3_QEMU` | `/home/derek/qemu-ma1co/build-arm/arm-softmmu/qemu-system-arm` | QEMU 可执行文件 |
| `A7M3_LOGDIR` | `/mnt/f/a7c2-re-main/a7m3-qemu-logs` | 串口/QEMU 日志目录（不存在会自动创建） |
| `A7M3_AVCAM` | `/home/derek/avcam403/av-cam.bin` | LIRO 固件 |
| `A7M3_PUMP` | `5` | 每条 guest 命令后继续收串口的秒数（长跑常设 100） |
| `A7M3_SEND_SO` | 空 | 插件路径；加载到 PA `0x10000000` |
| `A7M3_QEMU_EXTRA` | 空 | 追加 QEMU 参数（QMP/gdbstub） |

## 1. `a7m3-shload.py` — 主 harness

```sh
python3 a7m3-shload.py <IMG> <TAG> <CMDFILE> [WAIT] [AVCAM] [AVCAM_ADDR]
```

- 启动 QEMU，连接 `unix:/tmp/a7m3-<TAG>.sock` 串口；
- 等 `/ #` 出现；
- 逐行发送 `CMDFILE` 中的 guest 命令；
- 把串口写 `<LOGDIR>/a7m3-<TAG>.log`，QEMU stderr 写 `.err`；
- `WAIT` 是启动后额外等待秒数（默认 30）。

常与 `a7m3-cmds-ui1.txt` 搭配：

```text
sleep 5; mount -t tmpfs none /tmp; busybox dd if=/dev/mem bs=4096 skip=65536 count=2 of=/tmp/v.so; /usr/bin/scenario.elf ../../tmp/v b:0x01; sleep 60; echo DONE
```

## 2. `a7m3-shellrun.py` — 跑任意 shell 命令

```sh
python3 a7m3-shellrun.py <IMG> <TAG> "cmd1" "cmd2" ...
# 或从文件读命令（@ 前缀）
python3 a7m3-shellrun.py <IMG> <TAG> @commands.txt
```

每条命令用 `BEGIN_<tag>_$$`/`END_<tag>_$$` 包裹，脚本把中间输出打印出来。
适合列目录、拷文件、确认模块/进程。

## 3. `watch_pm.py` — QMP 内存抓取

需要 QEMU 以 `-qmp unix:/tmp/a7m3-qmp7.qmp,server=on,wait=off` 启动（通过
`A7M3_QEMU_EXTRA`）。脚本等日志出现 `SCN done`，延时 40 秒后依次 `pmemsave`：

| 地址 | 大小 | 文件 |
|---|---|---|
| `0x202e0200` | `0x96000` | `/tmp/a7m3-pm-disp.raw` |
| `0x226e0000` | `0x100000` | `/tmp/a7m3-pm-tiles.raw` |
| `0x22770000` | `0x20000` | `/tmp/a7m3-pm-mini.raw` |
| `0x228f0000` | `0x10000` | `/tmp/a7m3-pm-tex228f.raw` |
| `0x222e0200` | `0x96000` | `/tmp/a7m3-pm-bg.raw` |
| `0x2055f000` | `0x40000` | `/tmp/a7m3-pm-gpu1.raw` |
| `0x20530000` | `0x40000` | `/tmp/a7m3-pm-gpu2.raw` |

环境变量：`PM_QMP`（默认 `/tmp/a7m3-qmp7.qmp`）、`PM_LOG`（默认
`/mnt/f/a7c2-re-main/a7m3-qemu-logs/a7m3-uiprobe8.log`）。

## 4. gdbstub 客户端

- `rsp_watch.py`：固定 VA 的 `Z2` 观察点（早期版本；VA 每次启动会变，慎用）；
- `rsp_kick.py`：从日志动态取 HKICK 的 VA 并下 `Z2`；
- `rsp_kick2.py`：同上，但命中后删除观察点，避免 memset 循环反复触发。

用法示例：

```sh
QEMU ... A7M3_QEMU_EXTRA="-gdb tcp::1234"   # 用 harness 启动
python3 rsp_kick2.py <LOGDIR>/a7m3-<TAG>.log 300 /tmp/rsp_kick.log
```

注意：

- `-gdb` 连接后 VM 会停在 `T02`，客户端必须先发 `c` 恢复；
- 命中后若直接继续，会反复命中同一条 libc 指令；`rsp_kick2.py` 会 `z2` 删除；
- 当前已知第一次命中是 libc 的拷贝/清零，不是命令构建者（见
  `../../evidence/05-gdbstub-watchpoint.txt`）。
