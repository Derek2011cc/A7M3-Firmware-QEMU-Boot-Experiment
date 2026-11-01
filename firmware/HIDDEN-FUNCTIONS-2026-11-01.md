# A7M3 4.03 固件：隐藏功能静态分析（2026-11-01）

> 方法：对 4.04 升级包解包后的 rootfs（`firmware.tar_unpacked/0700_part_image/dev/nflasha15_unpacked`）
> 做字符串/资源/注册表静态分析；**未在真机上执行**。结论分“机制已确认”和“仅资源存在”两档。

## 1. 结论摘要

A7M3 固件里存在三类“正常 UI 不暴露、但机制/资源在”的功能：

1. **服务/工厂模式**：JIRITSU 服务协议 + MPR 工厂/测试命令，可通过 USB 服务模式访问；
2. **隐藏视图**：资源与 `global.xdb` 注册表里存在，但正常菜单没有入口（调整/测试/修复类）；
3. **区域/型号门控**：语言、目的地行为由 `ModelIdTable` / `RegionID` / `RegionInfo` 控制。

## 2. 服务/工厂机制（binary 字符串证据）

### 2.1 JIRITSU 服务协议（`libObj.so`）

```
JIRITSU_START / JIRITSU_MODE / JIRITSU_MOUNT
JIRITSU_GET_FILE / JIRITSU_GET_FILE2 / JIRITSU_GET_FILELIST / JIRITSU_GET_FILE_HASH
JIRITSU_SET_FILE / JIRITSU_SET_FILE2 / JIRITSU_SET_FILE_PATH
JIRITSU_FILE_DELETE / JIRITSU_FILE_DELETE2
JIRITSU_FORMAT / JIRITSU_FIRMUP / JIRITSU_COMPARE_MEDIA
JIRITSU_DEBUG / JIRITSU_FACTORY / JIRITSU_GET_VERSION
```

含义：服务模式（USB `054c:0336` 一类）下，固件支持**读/写/列目录/取哈希、格式化、固件更新、
介质比对、DEBUG/FACTORY 模式**。这是 PMCA/OpenMemories 类工具能工作的底层协议。

### 2.2 MPR 工厂/测试命令（`libObj.so` / `libmpr*.so`）

| 类别 | 命令 |
|---|---|
| 工厂模式 | `MPR_CMD_SET_FACTORY_MODE`、`MPR_CMD_GET_FACTORY_MODE_RESULT`、`MPR_SCN_SET_FACTORY_MODE`、`MPR_SCN_EXEC_FACTORY_MODE`、`EXEC_FACTORY_RESET`、`READ_FACTORY_RESET_STATUS`、`FACTORYDEFAULT_BACKUP_CONFIG` |
| 调整值 | `EXEC_SYS_SET_ADJUSTMENT_VALUE` |
| 器件测试 | `MPR_CMD_SET_DEVICE_TEST`、`SET_DEVICE_TEST_ENV`、`OUTPUT_DEVICE_TEST_RESULT` |
| JET 测试 | `MPR_CMD_JET_TEST_START/STOP/RESULT`、`JET_AUTO_TEST`、`JET_AUTO_TEST_RESULT` |
| 无线/RF | `MPR_CMD_SET_RF_TEST_MODE`、`SET_WIFI_TEST_MODE`、`EXEC_WIFI_TEST` |
| 传感器 | `MPR_CMD_GSENSOR_CHECK`、`GSENSOR_ADJUST_START/STOP`、`GSENSOR_GET_VALUE/SET_DROP` |
| 介质/温度 | `MEDIA_TEMP_ADJUST_START/STOP`、`GET_VALUE/SET_VALUE`、`CHECK_MEDIA_*`、`FORMAT`、`MAKE_PARTITION` |
| 文件/日志 | `MPR_CMD_READ_DATA_FILE`、`WRITE_DATA_FILE`、`GET_RW_DATA_FILE_STATUS`、`SAVE_DRIVERLOG` |
| 镜头/配置 | `MPR_CMD_PMBP_CHECK_DATA`、`GET_STATUS`、`WRITE_DATA`、`MSEN_BACKUP_RELOAD` |
| WLAN 工厂 | `WLAN_GEN_DYNAMIC_FACTORY_INFO*`、`WLAN_*_GO_DYNAMIC_FACTORY_*` |

### 2.3 外部命令执行

`libExternalCommand.so` 使用 `stm_popen`/`stm_pclose`，固件内部可执行外部命令（诊断用）。
正常 UI 不可达，但说明“有 shell 执行通道”。

## 3. 隐藏视图（资源 + global.xdb 注册，正常菜单无入口）

| 视图 | 用途（推断） | 备注 |
|---|---|---|
| `viewAfAdjustment` | AF 调整 | 服务调整 |
| `viewEvfAdjustment_D` | EVF 调整 | 服务调整 |
| `viewMotionAdjustment` | 防抖/陀螺调整 | 服务调整 |
| `viewCalibration` | 校准 | 服务 |
| `viewGpsTest` / `viewGpsInfo` / `viewGpsTracker` | GPS 测试/信息 | A7M3 无 GPS 硬件，疑似共用平台代码 |
| `viewWlanTest` | WLAN 测试 | 工厂测试 |
| `viewWidgetTest` | 控件测试 | 工厂测试 |
| `viewTestCommonFunctions` | 测试通用功能 | 工厂测试 |
| `viewAutoRepair` / `viewManualRepair` / `viewManualRepairForPlay` / `viewSalvageRestore` | 自动/手动修复、抢救恢复 | 恢复类 |
| `viewBatteryAuthentication` | 电池认证 | 检测/提示 |
| `viewCertificationLogo` | 认证标志 | 法规信息 |
| `viewUserUpdate` | 固件更新 | 正常菜单可达 |
| `viewUpdateRootCertificate` | 根证书更新 | 隐藏 |
| `viewMotionVideo` | 运动视频 | 可能为其它型号 |
| `viewTcPreset` / `viewUbPreset` | 时间码/用户位 | A7M3 菜单无 TC，疑似共用 |

## 4. 区域 / 语言 / 型号门控

- `libObj.so` 内：`ModelIdTable`、`ModelIdSoTable`、`RegionID`、`RegionInfo`、`inhUnmatchRegioncode`；
- 资源内有 20+ `string_<lang>.uxc`（英/日/简中/繁中/韩/俄/阿/泰/越等）；
- 地区/型号不匹配时，语言列表、默认设置、PAL/NTSC、录像时长限制等可能不同——需在 QEMU 里改配置验证。

## 5. “可开启”的路径与风险

| 路径 | 能做什么 | 风险 |
|---|---|---|
| USB 服务模式（JIRITSU） | 读写文件、取日志/哈希、格式化、固件更新、进入 DEBUG/FACTORY | **高**：`SET_FILE`/`FORMAT`/`FIRMUP` 可致砖 |
| 工厂模式（MPR `SET_FACTORY_MODE`） | 打开调整/测试视图，读写调整值 | 中高：调整值会影响校准/保修 |
| 场景命令（`MPR_SCN_*`，testcmd） | 在 QEMU 内触发工厂/测试流程 | 低（离线） |
| 修改配置/IMDB（地区、语言） | 解锁语言/地区行为 | 中：配置错乱需恢复 |
| 直接 `openView("view/...")`（QEMU） | 打开隐藏视图看界面 | 低（离线） |

## 6. 待验证

- 每个隐藏视图在 A7M3 上是否真正可用（有些是共用平台/无硬件）；
- 工厂模式是否持久化、如何退出；
- 地区/语言门控的具体字段位置（IMDB / backup / config）；
- JIRITSU 命令的完整参数格式（需要 USB 抓包或 PMCA 源码对照）。

## 7. 复现命令

```bash
ROOT=/path/to/nflasha15_unpacked
strings -n 6 $ROOT/lib/libObj.so | grep -E 'JIRITSU_|MPR_CMD_|FACTORY'
find $ROOT -iname 'view*.uxc' | grep -iE 'adjust|test|repair|calib|gps|wlan|widget'
```

## 8. QEMU 内打开隐藏视图的尝试（2026-11-01）

做法：写 `scripts/guest/scn_hidden.c`，从 `/tmp/viewlist` 读取视图名，调用
`viewManagerIf::openView("view/<name>", 0)`，逐个尝试。

结果（三次独立运行）：

| 视图 | 结果 |
|---|---|
| `VersionNumber`（普通视图，对照） | `openView` 内部 **page translation fault @ 0x10**，App 进异常监视器 |
| `AfAdjustment`（隐藏） | 同上 |
| `GLOBAL_MENU`（根视图，对照） | 同上 |

日志特征：`SCNH enter` → `SCNH open` → **无 `SCNH ret`** → `Exception Syndrome`，
`SUGI-SD` 0 条。

**结论**：`openView` 不能从 testcmd 线程调用——它依赖 UI 线程/父屏幕上下文，
不是“视图名不对”。要打开隐藏视图，需要：
1. 在 **UI 线程回调** 里调用（hook 一个每帧/按键回调）；
2. 或走 **工厂模式**（MPR `SET_FACTORY_MODE`）让 App 自己切换；
3. 或 **修改菜单资源**（把隐藏视图注册成正常菜单项）；
4. 真机上则走 USB 服务模式。

仅凭“知道视图名 + openView”在 QEMU 里打不开隐藏视图。

### 8.1 UI 线程 hook 尝试（2026-11-01 续）

| 尝试 | Hook 点 | 结果 |
|---|---|---|
| 1a | `libObj+0x73a100`（2D scheduler） | hook 安装并触发（`UIH hook`），openView 在**该线程**调用 → 仍然 `0x10` 崩溃；说明 2D 线程不是 UI 线程 |
| 1b | `osal_rcv_msg_tmo`（消息循环） | 函数开头不是普通 push 序列（`w0=2d4f00f4`、`w1=8943f0e9`），8 字节 trampoline 复制破坏了原指令 → `dcache parity error`；openView 未执行到，**结论不确定** |

下一步若要继续这条路线，应改用 **GOT/PLT 级替换**（不改函数序言）：扫描 libObj 可写段中等于
`osal_rcv_msg_tmo` 地址的 GOT 项，替换为 hook，hook 内直接调用原地址；或用已知有干净序言的
UI 线程函数。

## 9. 工厂模式的官方入口：/scenario 模块（2026-11-01）

固件根目录 `/scenario/` 下有 35 个场景模块，其中直接相关：

```
MPR_SCN_SET_FACTORY_MODE.so      (5,766 B)
MPR_SCN_EXEC_FACTORY_MODE.so     (6,192 B)
MPR_SCN_FORMAT.so
MPR_SCN_FLASH_BACKUP_INIT.so
MPR_SCN_EXEC_WIFI_TEST.so
MPR_SCN_GET_TOTAL_LOG.so
```

`scenario.elf` 的用法（QEMU 内实测输出）：

```
Usage: /usr/bin/scenario.elf <options> [scenario_name] [size]:[data] ...
    scenario_name: /usr/scenario/xxxx.so
options: --ifile ifile / --obfile obfile
```

`MPR_SCN_SET_FACTORY_MODE.so` 的 `scenario_run`：要求 `arg2 > 3`，构造 0x20 字节请求
（头部 + 从输入复制的 8 字节），用 `MWF::EventWrapper::SendAndRecv` 发送，
send id 常量 = `0x011502f7`。

**QEMU 实测**：

- `scenario.elf /scenario/MPR_SCN_SET_FACTORY_MODE.so` → 正常返回（`SCN_FM`），**无异常**；
- 带 `8:00000001` / `8:01000000` / `4:00000001` 参数 → 同样正常返回；
- 但 **`SUGI-SD` 0 条**，没有工厂 UI 画面。

结论：官方入口已经找到（场景模块 + scenario.elf），且不会像 `openView` 那样崩；
但 8 字节 payload 的语义还没解出来，或工厂模式需要 MPR 服务处于服务模式/重启后才生效。
下一步应解 `MPR_SCN_EXEC_FACTORY_MODE.so` 的 `ExecMprUtility` 命令 ID 和 payload。

### 9.1 直接调用场景模块（2026-11-01 续）

路径纠正：场景模块在客体的 **`/usr/scenario/`**（不是 `/scenario/`）。

插件 `scripts/guest/scn_fm2.c`：先 `dlopen("/usr/lib/libmpr.so", RTLD_GLOBAL)` 等依赖，
再 `dlopen("/usr/scenario/MPR_SCN_SET_FACTORY_MODE.so")`，`dlsym("scenario_run")`，
用 8 字节 payload 调用 `scenario_run(buf, 8, 4)`。

QEMU 实测：

- `HSET = 0x000487c0`（模块加载成功）；
- `RET = 0x00000004`（场景返回 4，表示请求已发送）；
- 仍然 `SUGI-SD` 0 条，没有立即出现工厂 UI；
- 期间出现 eMMC 写入重试错误（客体在写盘，可能与工厂标志有关）。

结论：**直接调用官方工厂场景模块是可行的**，请求已发出；下一步是等待/重启后看 UI，
以及解出 8 字节 payload 的语义（当前用 {0,0} 也返回 4）。
