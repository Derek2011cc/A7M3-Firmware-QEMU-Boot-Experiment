# FAT12 分区工具

镜像 `qemu-emmc-2g.sen.img` 在文件偏移 `0xb780000` 有一个 FAT12 分区，参数：

```text
SS=512, SPC=32, RESV=1, NFATS=1, ROOTENTS=512, FATSZ=3, TOT=24576
```

里面放预加载 shim：

- `/etc/ld.so.preload`（16 B，内容 `/etc/shim_io.so\n`）
- `/etc/shim_io.so`（39,228 B，即 `payload/shim_uipc.so`）

| 脚本 | 作用 |
|---|---|
| `fat12_ls.py [img]` | 列 FAT 根目录 |
| `fat12_cat.py [img] <8.3-name>` | 读文件到 stdout（只写一个参数时用默认镜像） |
| `fat12_add2.py [img] [shim.so]` | 把 `SHIM_IO.SO` 和 `LDSOPRE.LOA` 加到 FAT 根目录（默认用 `payload/shim_uipc.so`，**原地修改**） |

示例：

```sh
cp qemu-emmc-2g.img work.img
python3 fat12_add2.py work.img payload/shim_uipc.so
python3 fat12_cat.py work.img SHIM_IO.SO | sha256sum   # 应与 payload 一致
```

**只对镜像副本操作**。不要对真实相机的分区执行写操作。写之前备份镜像；
写后用 `fat12_cat.py` 读回并比对 SHA-256（本仓库的判据纪律：写入必须读回校验）。

实测（2026-11-01，在 `qemu-emmc-2g.img` 的副本上）：

```text
shim size 39228
shim_io.so cluster 2 slot 25 size 39228
ld.so.preload cluster 291 slot 27 size 16
读回 SHA-256 = 0c22aa20414e9ac3eafde1028588e523b555acce529a7802155f23ccea95c3bd
与 payload/shim_uipc.so 一致
```
