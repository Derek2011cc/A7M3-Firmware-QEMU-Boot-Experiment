# Sugilite GPU 卡点与绕过路线

> **2026-11-01 更新**：原生菜单视图已经能在 `im.elf` 内打开，GPU 命令流已抓取
> 并在 QEMU 内软件重放（离线帧）；但像素还不正确。命令语法、LZ77、软件重放和
> 未解问题的详细记录见
> [../ui-rendering/UI-RENDERING.md](../ui-rendering/UI-RENDERING.md)。
>
> **2026-10-31 晚更新**：GPU 时钟/设备节点和 DFB 崩溃都已绕过，
> Appframework 现在可以运行到 **63 线程**。当前阻塞不再是 GPU 初始化，
> 而是 LIRO/CP 的 `0xf2a1xxxx` 硬件忙等、`bootin.elf` 的 UIPC 等待，
> 以及完全没有 Sugilite 显示模型。最新细节见
> [LIRO-AND-MEMORY.md](LIRO-AND-MEMORY.md)。

本文保留 GPU 卡点的历史，并记录更新后的现状、可选路线和工作量。

> **设备名更正**：实际设备节点是 **`/dev/dmpgles2`**（字符设备
> `248:29`），不是 `/dev/sugilite`。模块通过 `udif_devnodes` 的
> `dmpgles2` 条目取 dev_t；`sugilite` 是驱动内部/日志里的名字。
> `devno.ko` 导出的 `udif_devnodes` 表见实验记录。

## 1. 已确认的事实

1. 原版 `grm_gles.ko` 加载时在 `sugi_hw.c:221` 触发 `BUG()`。
   `init_module` 在 pid 1 的上下文执行，`BUG()` 直接杀死 init。
2. 把 `init_module` 整个换成 `mov r0,#0; bx lr` 后模块加载成功，但
   **不注册字符设备**（注册代码在 init_module 里），因此 Appframework
   打不开设备。
3. `grm_gles.ko` 通过 `cdev_add` 注册 `dmpgles2`，dev_t 来自
   `udif_devnodes`：**major 248, minor 29**。模块本身不创建 `/dev` 节点；
   相机 rootfs 的 `/dev` 是静态 ext2（未挂 devtmpfs），所以节点要离线加。
4. 原版驱动在 `sugilite_clock_up_internal` 里读 GPU 时钟寄存器，等不到
   `0x15` 就执行 `__bug_noarg`。只把该函数的最终检查改成“返回成功”，
   并恢复原始 `init_module`，模块的 init 就能继续并注册 cdev。
5. 给 rootfs 加上 `/dev/dmpgles2 c 248 29` 后，Appframework 能打开并
   `mmap` 该设备（128 KiB），`FixedAreaTop/Size` 也从 `FFFFFFFF` 变成
   `20098000/0007C000`。
6. 新的崩溃点不在 `/dev` 打开，而在用户态内存管理器：
   `__pthread_mutex_lock` 收到 `r0=0x1c`（NULL `this+0x1c`），调用者位于
   `libmpr.so`（一次符号化为 `MemInfo::StreamAreaFree`），线程
   `THREAD_INFRA1`，页故障地址 `0x28`。（根因见第 8 条：DFB 错误路径
   二次加锁，不是 FixedArea 内容本身。）
7. `libmpr.so` 里把该函数入口改成 `bx lr` 的补丁**没有效果**；ASLR 下
   不同次启动的调用点和绝对地址会变化，说明不能靠单点补丁解决。
8. 后续反汇编确认，直接崩溃点是 **DFB 在 1 扇区的 `nflasha4` 上写失败后，
   错误路径二次加锁 MemInfo 互斥量**，触发 glibc 2.11.2
   `pthread_mutex_lock.c:62` 断言。把 DFB 打开路径改到 `/dev/nflasha16`
   后，断言和 trim 错误消失，`im.elf` 稳定运行到 **63 线程**。
9. 63 线程下 `libmpr.so`/`libmprctrl.so`/`libmprdummy.so` 已映射，但
   `viewUnified8.so`/`libgrm.so`/`libuxengine.so`/`libDDS.so` 仍未加载；
   `bootin.elf` 3 线程，wchan `osal_rcv_msg_tmo`（UIPC 等待）。
10. LIRO 固件（`av-cam.bin`）能执行 `entry_pre`/`entry_main`，但随后在
    Thumb 代码 `0x63a1d294` 忙等 `0xf2a1102c` bit1；QEMU 未建模
    `0xf2a1xxxx`，读回 0，RT 线程抢死 Linux。

结论：**GPU 设备节点/时钟绕过已经完成；DFB 崩溃也已绕过。当前阻塞是
LIRO/CP 硬件忙等、UIPC 握手和显示模型，而不是 FixedArea 的 NULL 指针。**

## 2. 路线 A：最小设备 + DFB 绕过（已完成，新阻塞在 LIRO/CP）

已完成的步骤（脚本在 `../scripts/experimental/`）：

1. `patch-grmgles-clock.py`：恢复原始 `init_module`，把
   `sugilite_clock_up_internal` 的 `cmp r0,#0x15; ldmeq` 改成
   `mov r0,#0; ldm`，让模块继续执行并注册 cdev；
2. `add-dmpgles2-node.py`：在 nflasha7 的静态 `/dev` 里加
   `dmpgles2 c 248 29`；
3. 启动后确认 `grm_gles` init 返回 0、`/dev/dmpgles2` 存在，
   Appframework 打开并映射 128 KiB；
4. `patch-libmpr-dfb16.py`：把 DFB 的打开路径从 `/dev/nflasha4` 改到
   `/dev/nflasha16`，避开 1 扇区分区写失败后的二次加锁断言。

结果：`FixedAreaTop=20098000`、`FixedAreaSize=0007C000`；
`im.elf` 稳定运行到 **63 线程**，`libmpr` 等已映射；不再出现
`pthread_mutex_lock` 断言。`viewUnified8.so`/`libgrm.so`/
`libuxengine.so`/`libDDS.so` 仍未加载，`bootin.elf` 阻塞在
`osal_rcv_msg_tmo`。

下一步：

- 处理 LIRO/CP 的 `0xf2a1xxxx` 硬件忙等（见路线 E）；
- 检查 `bootin.elf` 的 UIPC 队列，确认它等待的消息；
- UIPC 握手推进后，再观察 UI 库是否加载。

## 3. 路线 B：QEMU 侧模拟 Sugilite

目标：在 QEMU 中实现 Sugilite 的寄存器/中断/DMA/显存模型，让原版
`grm_gles.ko` 自己完成初始化。

需要：

- 从 `grm_gles.ko` 的 MMIO 访问反推寄存器窗口和语义；
- 时钟/复位/PLL 状态机（`sugi_clock_up_internal` 等待的条件）；
- 中断控制器连接；
- 显存与扫描输出；
- 与 Sony display/VI 子系统的交互。

工作量：**数周到数月**，且寄存器语义无公开文档，失败风险高。
即使 GPU 初始化通过，还需要显示控制器和面板模型才能看到画面。

## 4. 路线 C：绕过 Appframework，只保留 shell

- 不启动 `im.elf`（改 init 或改名），系统停在 BusyBox。
- 可研究内核模块、分区、用户态工具、网络/USB 等。
- 没有菜单/UI，也没有拍摄链。

工作量：小时级。收益：稳定的 shell 环境，适合做其它离线实验。
这不是“原生相机”。

## 5. 路线 D：替换 `im.elf` 为桩

- 用一个只睡眠/等待的小程序替换 `im.elf`，init 不会崩，`Exception>`
  也不再出现。
- 仍然没有 UI；只是让系统看起来“活着”。

工作量：小时级。收益主要是避免每次手动 `exit`。

## 5.1 路线 E：LIRO/CP 的 `0xf2a1xxxx` 最小寄存器模型

事实（2026-10-31 晚）：

- LIRO 固件在 `entry_main` 内初始化 `0xf2a11000`/`0xf2a12000` 两个实例：
  写 `0xf2a11020 |= 0x100`、`0xf2a11024 = 1`，然后循环读
  `0xf2a1102c` 等 bit1；
- QEMU 没有建模该块，读回 0；
- 固件在 RT 线程中忙等，抢死 console、shell 和 Appframework；
- 把该 `beq` 改成 NOP（`av-cam-nowait1.bin`）只能跳过这一处，后面还有
  其它等待。

建议做法：

1. 在 QEMU 中加一个最小寄存器块（`0xf2a11000` 和 `0xf2a12000` 各一段），
   写入保存、读回保存值；
2. `+0x2c` 先返回 bit1（以及后续观察到的状态位），让当前等待通过；
3. 用 GDB 记录固件下一次停住的 PC/寄存器，逐个补状态位；
4. 状态机稳定后，再检查 LIRO 是否向 UIPC 队列写入消息，以及
   `bootin.elf` 是否收到。

工作量：**数天到数周**（取决于等待点数量）。这是最小 CP 模型，不是完整
LIRO 核；寄存器语义仍需从固件行为反推。

## 6. 建议顺序

1. **已完成**：`dmpgles2` + 时钟绕过 + DFB `/dev/nflasha16` 绕过；
   `im.elf` 已能稳定运行到 63 线程。
2. **路线 E**：给 LIRO/CP 的 `0xf2a1xxxx` 做最小寄存器模型，先让
   `0xf2a1102c` bit1 可读，再用 GDB 逐个定位后续等待点。
3. 检查 `bootin.elf` 的 UIPC 队列与 LIRO 侧端点，确认它等待的消息；
   必要时构造最小握手。
4. **路线 B**：显示模型（Sugilite/panel/ramfb）另行评估；不要一开始做
   完整 GPU。
5. C/D 只作为调试便利，不作为目标。

## 7. 判断“绕过成功”的标准

- `/dev/dmpgles2` 可打开、可 mmap（已达成）；
- Appframework 不再在 `libmpr` 的互斥量/DFB 路径崩溃（已达成）；
- 进程能继续执行到下一个可观察的初始化阶段（**已达成：63 线程**）；
- 进一步判据：UI 库（`viewUnified8.so` 等）被加载，
  `bootin.elf` 离开 `osal_rcv_msg_tmo`；
- 不要求出现画面。画面属于显示模型，另算。

## 8. 风险

- 这些改动只作用于本机生成的镜像/模块，不涉及真机刷写。
- 修改 `im.elf` 或模块会改变校验和/签名（如果有），仅用于离线实验。
- 不要把 Sony 二进制或修改后的二进制上传到公开仓库。
