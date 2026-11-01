# cam-shell — A7M3 服务模式 shell 工具

通过 USB **Senser 服务模式**连接 A7M3，在相机上执行命令/传文件。不刷固件、不改分区；
每次会话结束后相机会回到 `054c:0c02`（Mass Storage）。

## 组成

| 文件 | 作用 |
|---|---|
| `cam-shell.cmd` | Windows 入口（双击/命令行） |
| `cam-shell.ps1` | 检查 usbipd、确保自动 attach、转调 WSL |
| `cam_shell.py` | WSL 内运行：Senser 认证 + 终端/文件操作 |

## 前提

- Windows 10/11 + WSL2（Ubuntu）；
- `usbipd-win`（已装，`C:\Program Files\usbipd-win\usbipd.exe`）；
- 相机 USB 连接方式设为 **Mass Storage**（不是 MTP/Auto）；
- WSL 内以 root 运行（脚本内部用 `wsl -u root`），并预装 PMCA 依赖：
  `pyusb pycryptodomex axmlparserpy asn1crypto pycparser==2.17 pyyaml tlslite-ng`；
- PMCA 源码在 `PMCA_PATH` 或默认
  `/mnt/f/a7c2-re-main/tmp/pmca-src/Sony-PMCA-RE-master`。

## 用法

```bat
cam-shell.cmd raw
```
进入相机命令模式：**没有 `cam>` 前缀**，直接输入命令；
每行发送到相机并打印约 3 秒输出，相机自己的 `/usr/share/app #` 提示符会出现在输出里。
`exit` / `quit` 退出本工具（不发送给相机），Ctrl+C 也可退出。

```bat
cam-shell.cmd shell
```
进入 PMCA 平台 shell，支持 `help`、`shell`（相机终端）、`push`、`pull`。

```bat
cam-shell.cmd cmd "uname -a"
cam-shell.cmd cmd "ls -l /usr/scenario" --seconds 5
```
一次性执行命令并打印输出（默认读 6 秒）。

```bat
cam-shell.cmd push payload\vfm2.so /tmp/v.so
cam-shell.cmd pull /tmp/cam_test.txt .\out.txt
```
上传/下载文件（`/tmp` 是 tmpfs，重启即消失）。

## 已验证

- `cmd "uname -a"` → `Linux (none) 3.0.27_nl-rt106+ ... armv7l`；
- `push _test.txt /tmp/cam_test.txt` + `cmd "cat /tmp/cam_test.txt"` → 内容一致；
- 服务模式身份 `054c:0336`，认证后 `readHasp` 88 字节。

## 注意

- 工具内部会确保只有一个 `usbipd attach --auto-attach` 循环；切换服务模式时设备会重新枚举，
  没有这个循环会“等不到 0336”；
- 相机切服务模式后 Windows 盘符会消失，退出后恢复；
- 这是 **root shell**：不要执行 `FORMAT`、`FIRMUP`、`mkfs`、工厂重置等写操作；
- 只在你自己的设备上使用。
