# 2026-11 最新研究进展

本文汇总 2026-11-01 前后与 A7M3 固件 QEMU / 服务模式 / 菜单渲染相关的最新结果，
便于从主仓库快速了解现状。更早的过程见 [../STATUS.md](../STATUS.md)、
[../DESIGN.md](../DESIGN.md)、[../ui-rendering/FINDINGS-2026-11-01.md](../ui-rendering/FINDINGS-2026-11-01.md)。

## 1. QEMU 启动到 shell：已拆成“下载即用”子仓库

- 内核 + rootfs 可以稳定启动到相机 BusyBox shell（`/ #`）；
- 启动链、补丁与手动复现仍在本仓库：[BUILD.md](../BUILD.md)、[patches/qemu-a7m3.patch](../patches/qemu-a7m3.patch)；
- **开箱即用版本**已拆到子仓库
  [a7m3-QEMU-shell](https://github.com/Derek2011cc/a7m3-QEMU-shell)：
  包含内核、initrd、压缩分片的 2 GiB eMMC 镜像、预编译 QEMU，以及浏览器串口终端；
  首次运行自动还原镜像，不需要自己准备固件或编译。
- 子仓库已验证：全新 clone 后 `./run.sh web` 能进入 `/ #` 并执行命令。

## 2. 服务模式（Senser）与相机 shell

- 使用 **Sony-PMCA-RE** 的 `senserShellCommand()` 完成两阶段 Senser 认证：
  `054c:0c02`（Mass Storage）→ `054c:0336`（服务模式）；
- 在真机上验证通过：
  - `SERVICE MODE OK`
  - `readHasp()` 返回 **88 字节**
  - 进入相机 BusyBox 1.34.1 shell，`/usr/share/app #` 可执行命令；
- Windows 侧通过 **usbipd-win + WSL2** 直通，不需要 Zadig/替换海量存储驱动；
- 工具已独立为 [a7m3-connect-shell](https://github.com/Derek2011cc/a7m3-connect-shell)：
  `raw`（逐行命令）、`cmd`、`push`、`pull`。

细节与风险见 [../firmware/HIDDEN-FUNCTIONS-2026-11-01.md](../firmware/HIDDEN-FUNCTIONS-2026-11-01.md)
以及子仓库 README。

## 3. 工厂模式（Factory Mode）实验

固件中存在官方入口：

```
/scenario/MPR_SCN_SET_FACTORY_MODE.so
/scenario/MPR_SCN_EXEC_FACTORY_MODE.so
```

`scenario_run(data, size, arg2)` 要求 `arg2 > 3`，构造 0x20 字节请求，
send id 为 `0x011502f7`。

已尝试（QEMU）：

- `scenario.elf /scenario/MPR_SCN_SET_FACTORY_MODE.so` → 返回 255/2，未真正发送；
- 自写插件直接 `dlopen` 模块并调用 `scenario_run(payload, 8, 4)` → 返回 **4（已发送）**；
- 仍未观察到工厂 UI；eMMC 出现写入重试，疑似写入了工厂标志，可能需要重启生效；
- 从 testcmd 线程调用 `viewManagerIf::openView` 会空指针崩溃（`0x10`），
  2D 调度器线程同样不行；需要 UI 线程 hook 或工厂模式由主线程切换。

结论：**入口已找到，payload 语义和生效时机仍未完全解出**，详见
[HIDDEN-FUNCTIONS §8–§9](../firmware/HIDDEN-FUNCTIONS-2026-11-01.md)。

## 4. 隐藏功能静态分析

对 4.04 更新包解包后的 rootfs 做字符串/资源分析，确认：

- **JIRITSU** 服务协议：文件读写/列表/哈希、FORMAT、FIRMUP、DEBUG、FACTORY；
- **MPR 工厂/测试命令**：SET_FACTORY_MODE、JET_TEST、DEVICE_TEST、RF/WiFi TEST、
  GSENSOR/MEDIA_TEMP 调整、SAVE_DRIVERLOG 等；
- 隐藏视图：`viewAfAdjustment`、`viewEvfAdjustment_D`、`viewMotionAdjustment`、
  `viewCalibration`、`viewGpsTest`、`viewWlanTest`、`viewWidgetTest`、
  `viewAutoRepair`、`viewBatteryAuthentication` 等；
- 区域/型号门控：`ModelIdTable`、`RegionID`、`RegionInfo`。

全文：[firmware/HIDDEN-FUNCTIONS-2026-11-01.md](../firmware/HIDDEN-FUNCTIONS-2026-11-01.md)。

## 5. 原生菜单的像素级基准

- 用 HDMI 采集卡抓到真机 1080p 菜单画面，归一化为 640×480 基准；
- 实测 tab 分隔线间距 **86 px**，与模拟器 draw list 基本对齐（差 ~2 px）；
- 颜色/纹理仍是差距：真机 tab1 红色渐变、选中行为黑色；设置页选中行为橙色渐变，
  模拟器目前统一使用常量颜色；
- 参考图与对比图在
  [ui-rendering/artifacts/reference/](../ui-rendering/artifacts/reference/)；
- 采集与归一化说明：
  [ui-rendering/docs/CAPTURE-REFERENCE.md](../ui-rendering/docs/CAPTURE-REFERENCE.md)。

## 6. 当前边界

- 菜单文字/图标仍不可读（UV/顶点、纹理采样、混合未解）；
- 工厂模式未确认进入 UI；
- 不含拍摄链、CMOS/镜头/编码器；
- 所有实验在 QEMU 或只读服务模式转储上进行，不修改真机固件。

## 7. 下一步

1. 解 `0x0458/0x0459` 顶点/属性布局或实现 GLES 层软件光栅；
2. 解工厂模式 8 字节 payload 与生效条件；
3. 用真机参考图做逐区域像素 diff，补齐颜色/混合；
4. 把 UI 线程 hook 做成可复用的探针。
