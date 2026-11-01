# 离线分析脚本

这些脚本读取 **串口日志**（`.log`）或 **QEMU stderr**（`.err`）里的 dump，
或者 QMP `pmemsave` 的 `.raw` 文件，不连接相机/QEMU。大多数脚本把结果打印到 stdout。

依赖见仓库根 `requirements.txt`：`Pillow`、`numpy`、`capstone`。

## 1. `gpu/` — 命令流、LZ77、纹理、帧

| 脚本 | 输入 | 作用 |
|---|---|---|
| `cmda_parse.py <err> [n]` | `.err` | 打印前 n 个 `SUGI-CMDA` 块 |
| `cmda_job.py <err> <idx...>` | `.err` | 只打印指定块 |
| `scan_ops.py <err> [op...]` | `.err` | 找出包含指定 opcode 的块 |
| `sums.py <err>` | `.err` | 汇总 `SUGI-2DS` 子命令 |
| `sum2d.py` / `sum2d2.py` / `sum2d3.py` | `.err` | 汇总 2D 引擎 `SUGI-2D`（源/目的/flags/字节数） |
| `sumtex.py <err>` | `.err` | 汇总 `SUGI-TEX/DST/SRC/PTR` |
| `tex_addr.py <err> <log>` | `.err`+`.log` | 把 `0x0589` 纹理值、`<<3` 地址与 LZ77 目的地址对照 |
| `ldec_parse.py <log>` | `.log` | 解析 `LDEC>` 记录 |
| `ldec_sum.py <log>` | `.log` | 汇总 LZ77 目的地址/长度 |
| `gpu_view.py` | `/tmp/a7m3-pm-gpu1.raw` | dump 顶点/索引/常量区（路径是常量，可改） |
| `mini_view.py` | `/tmp/a7m3-pm-mini.raw` | 把 0x2277xxxx 小纹理转 PNG |
| `render_tex.py <raw> <base> <pa> <w> <h> <out>` | `.raw` | 通用纹理转 PNG（RGBA4444，线性） |
| `render8.py` | `/tmp/a7m3-pm-tex228f.raw` | 0x228fxxxx 区域的多格式试验 |
| `make_tiles.py <raw> <out>` | `.raw` | 按 64×64 tile 视图拼图 |
| `tile_tex.py` | `/tmp/a7m3-pm-tex228f.raw` | 单张 296×48 纹理的 tile 试验 |
| `conv_bg.py <raw> <out>` | `.raw` | 640×480 RGBA4444 → PNG |
| `rawstat.py <raw>` | `.raw` | 统计 16-bit 字的分布/唯一值 |
| `frames2gif.py [dir] [prefix]` | `frame_*.raw` | 逐帧 RGBA4444 → contact sheet + GIF |

### 关于 `0x0589 << 3`

`tex_addr.py` 是验证地址编码的关键脚本：它把 GPU 命令里的 `0x0589` 值左移 3 位，
与 shim `LDEC>` 记录中的 `m[2]`（目的物理地址）比对。两者精确吻合，说明
`0x0589` 不是句柄，而是 `物理地址 >> 3`。

### 关于纹理格式

`render_tex.py`/`make_tiles.py`/`tile_tex.py` 都只是**试验**：它们假设
RGBA4444 + 线性/简单 tile。已确认 `0x22772380`（56×48）的橙色砖纹在 RGBA4444
线性视图下可辨认；`0x22773900`（64×56）是黑底灰条的 UI 元素，不是砖纹。
96×72、296×48 仍然稀疏/像噪声。纹理格式与 tiling 尚未确定，不要把这些输出当成真值。

## 2. `libobj/` — 静态分析（libObj/ELF）

| 脚本 | 作用 |
|---|---|
| `plt_inv.py` | 解析 ELF 的 PLT/重定位，列出导入函数 |
| `bcall.py` | 在反汇编里找 `bl` 调用并解析目标 |
| `near8.py` / `nears.py` | 在二进制里找邻近的 8/多字节常量/引用 |
| `elf_plt_calls.py` | 统计 PLT 调用点 |
| `dis_range.py` | 用 capstone 反汇编指定 VA 范围 |
| `find_consts.py` | 搜索常量 |
| `find_refs.py` | 搜索指针引用（注意：对 `.rel.dyn` 无意义） |
| `find_str.py` / `str_ctx.py` / `str_ctx2.py` | 搜索字符串及上下文 |
| `find_menu.py` | 搜索菜单/view 相关字符串 |
| `dump_tmpl.py` / `dump_tmpl2.py` | dump 模板/表 |
| `dump_ro.py` | dump 只读段 |
| `list_gfxeng.py` | 列出图形引擎相关符号/调用 |
| `movscan.py` | 扫描 MOV 立即数 |

这些脚本的参数各不相同，多数直接读 `/home/derek/libObj.so` 等本机文件；
使用前看一眼文件头。它们用于找 `openView`、`GraphicsImage`、2D submit 等函数的
调用者和偏移，不是运行时工具。

## 3. 通用用法

```sh
# 命令流摘要
python3 gpu/cmda_parse.py /path/to/a7m3-uiprobe3.err 42

# LZ77 目的地址
python3 gpu/ldec_sum.py /path/to/a7m3-uiprobe3.log

# 逐帧出图
python3 gpu/frames2gif.py /tmp/a7m3-frames-center /tmp/menu
```

日志里的行可能非常长（命令 dump 一行 256+ 个字），建议重定向到文件再用编辑器看。
