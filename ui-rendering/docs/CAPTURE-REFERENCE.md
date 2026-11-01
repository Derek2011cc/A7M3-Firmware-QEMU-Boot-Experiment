# 真机菜单采集（像素级基准）

采集卡：**USB3 PLUS Video**（UVC，Windows DirectShow）。
工具：`ffmpeg`（winget 安装 `Gyan.FFmpeg` 9.0.2），`scripts/capture/capture_ref.ps1`。

## 相机设置

1. **HDMI Info. Display = 开**（设置 → HDMI → HDMI 信息显示）。
   关闭时 HDMI 只输出干净画面，菜单/OSD 不会进采集。
2. HDMI 输出分辨率 1080p（采集端按 1920×1080 抓）。
3. 自动关机 = 关（或 30 分钟），避免采集途中休眠。
4. 需要采集的页面先手工导航到，保持静止。

## 采集

```powershell
pwsh -File scripts\capture\capture_ref.ps1 -Name setup1
```

输出到 `artifacts/reference/real-<name>-1920x1080.png`。

## 归一化

A7M3 的 LCD UI 是 4:3；1080p HDMI 把 640×480 的 UI 放大为 **1440×1080 居中**（左右各 240 px 黑边）。

- 原始：`real-<name>-1920x1080.png`
- 裁切：`real-<name>-1440x1080.png`
- 对比用：`real-<name>-640x480-lanczos.png`

归一化脚本：`tmp/normalize_ref.py`（按 `-Name` 扩展）。

## 已采集

| 页面 | 文件 | 备注 |
|---|---|---|
| 设置1（toolbox 第 1 页） | `real-setup1-1920x1080.png` | 6 tab、标题“设置1”、1/7、6 行、橙色选中行、底部页码点 + MENU/返回 |

## 与模拟器帧对比

```bash
python3 tmp/diff_ref.py <ref-640.png> <emu-640.png> out.png
```

输出：MAE、分区域（tabs/title/rows/bottom）MAE、三栏对照图（真机 | 模拟器 | diff 热图）。

## 待采集页面

- 菜单根页（拍摄1/2）
- 设置1..7 各页
- 播放页
- 星标页
- 子菜单/帮助页（验证浮层与文字）
