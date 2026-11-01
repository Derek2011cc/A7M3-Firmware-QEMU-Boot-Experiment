# 脚本目录

```text
scripts/
├── boot/      启动 QEMU、发送 guest 命令、QMP/gdbstub 观察
├── guest/     guest 插件和预加载 shim 的 C 源码 + build-plugins.sh
├── analysis/  离线解析 GPU 命令流/LZ77/纹理/帧，以及 libObj 静态分析
└── fat/       eMMC 镜像内 FAT12 分区的读写/注入
```

## 依赖

- 宿主机：Python 3、`Pillow`、`numpy`、`capstone`（见上级 `requirements.txt`）；
- 编译 guest 插件：`clang`（含 ARM 后端）+ `lld`；
- 运行 QEMU：按 [../../BUILD.md](../../BUILD.md) 构建的 `qemu-system-arm`。

## 最小工作流

1. `guest/build-plugins.sh` 编译插件；
2. `boot/a7m3-shload.py` 启动 QEMU 并发送 `boot/a7m3-cmds-ui1.txt`；
3. `boot/watch_pm.py`（QMP）或 QEMU 的 `A7M3_SURF_DIR` 导出内存/帧；
4. `analysis/gpu/*` 解析日志、把帧转 PNG/GIF；
5. `analysis/libobj/*` 静态定位函数/调用者。

## 约定

- 所有脚本只操作本机文件/QEMU，不连接真实相机；
- 不把 Sony 固件、镜像、大体积日志提交到仓库；
- 修改脚本后至少做一次 `python3 -m py_compile` 或 `bash -n`，再提交；
- 相机侧执行的文本必须纯 ASCII。
