# STC32G12K128 — RT-Thread Nano / IPC / 横屏 GUI / USB CDC MSH

适用于 STC32G12K128 / TFT 480×320 V2 开发板的独立 Keil C251 工程。使用 **RT-Thread Nano 4.1.1** 内核和官方 FinSH/MSH，控制台通过 MCU 原生 USB CDC 虚拟串口连接电脑。

本版补充事件、消息队列、互斥量示例、软件定时器和动态线程创建，并加入 **480×320 横屏 GUI**：状态、IPC、MSH 三个页面。MSH 页面显示实际命令行输入回显和输出，不是预设文字。

**验证状态：已通过 C251 编译及电脑端软件测试，并已由项目维护者烧录到开发板上验证。当前 IPC、动态线程及横屏 GUI（含 MSH 页面）版本已完成板上验证。**

**验证硬件: 老梁开源示波器 V1.1 (STC32G12K128)**

## 使用

1. 用 Keil C251 打开 `RTThreadNano.uvproj`，或直接烧录 `release/RTThreadNano-IPC-GUI.hex`。旧文件名 `release/RTThreadNano-CDC-FinSH.hex` 保存同一份固件。
2. STC-ISP 下载设置为 **35 MHz**。工程保持 **251 Source Mode、XSMALL、4 字节中断栈帧、FUNCTIONS(REENTRANT)**，不要改为 8051 模式或 2 字节中断栈帧。
3. 复位后，通过 MCU 原生 USB 数据口连接电脑；在串口终端打开对应 COM 口，115200、8N1、无硬件流控，最好开启 DTR，关闭本地回显。USB 传输不依赖这个名义波特率。
4. 出现 `msh >` 后输入 `help`；没有提示符时按 Enter 或 Ctrl-C，重新打开串口也会刷新提示符。

附带的逐行 Windows 终端：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\usb_terminal.ps1 -Port COM7
```

未指定端口时，仅在发现唯一 `VID_34BF&PID_FF02` 串口时自动选择。此脚本的 `exit` 关闭电脑终端。要使用方向键历史、光标编辑和 Tab 补全，请使用支持 VT/ANSI 的串口终端。

## 横屏 GUI 与按钮

LCD 初始化使用与本板 `Cube3D` 相同的横屏配置：**MADCTL `0x2B`，列范围 0–479，行范围 0–319**。字体绘制也按 480×320 裁剪。

| 页面 | 内容 |
|---|---|
| STATUS | tick、心跳、按键计数、心跳周期、LCD 更新次数、动态线程状态 |
| IPC | 软件定时器回调次数、队列发送/接收/丢弃次数、最近样本、堆占用 |
| MSH | 最近 14 行、每行 76 列的实际控制台内容：输入回显、命令结果和提示符 |

只有 LCD 线程操作屏幕。切页时绘制框架，平时只更新变化的数值和文本行。MSH 使用 XDATA 文本环形缓冲区，没有全屏像素帧缓冲；切到其他页面仍继续收集输出，USB 未连接或发送超时时也保留 LCD 内容。处理换行、回车、退格及清屏/归位等常用 ANSI 控制，不是完整终端模拟器；字体当前只支持 ASCII。

| 按钮 | 操作 |
|---|---|
| P7.2 / 左 | 上一页，循环切换 |
| P7.1 / 右 | 下一页，循环切换 |
| P3.2 | 下一页 |
| P7.4 / 上 | 心跳周期缩短 100 ms，最小 100 ms |
| P7.3 / 下 | 心跳周期延长 100 ms，最大 1000 ms |
| P7.0 / 中 | 清零心跳与按键计数 |

按键位与 `Cube3D` 一致，低电平检测，约 30 ms 消抖。也可输入 `page msh` 切到控制台页，再执行 `help`、`ps`、`ipc`，LCD 同步显示结果。命令通过电脑串口输入，五向键用于 GUI 导航。

## 命令

| 命令 | 功能 |
|---|---|
| `help` / `version` | 命令列表 / 内核与板子版本 |
| `ps` | 线程优先级、状态、栈使用高水位 |
| `tick` / `stat` | 100 Hz tick / 心跳、按键、LCD 计数和周期 |
| `beat 200` | 心跳周期 100–1000 ms，步长 10 ms |
| `ipc` | 消息队列、软件定时器、动态线程及事件/互斥量用途 |
| `timer 100` | 软件定时器周期 10–1000 ms，步长 10 ms |
| `timer off` / `timer on` | 停止软件定时器 / 按 200 ms 周期启动 |
| `dyn stop` | 请求动态心跳线程退出，在下次唤醒时完成 |
| `dyn start` | 重新动态创建心跳；已运行或栈槽占用时返回错误 |
| `dyn status` | 动态线程和 EDATA 栈槽状态 |
| `mem` | XDATA 堆总量、占用、峰值和剩余量 |
| `page status` / `page ipc` / `page msh` | 切页，也支持 `page 0` / `1` / `2` |
| `page` | 当前页面编号 |
| `reset` | 清零心跳/按键/LCD 计数，不复位 MCU |
| `clear` | 清除 USB 终端和 LCD 的 MSH 文本内容 |
| `echo "hello STC"` | 回显参数，支持引号 |

支持退格、左右光标、Tab 补全、上下键历史（2 行）、Ctrl-C 取消输入。输入行达到 64 字节会整行丢弃，不执行剩余片段。MSH 命令模式不包含 FinSH C 表达式解释器、文件系统或动态模块。

## 组件如何工作

实际示例在 `app/main.c`，不是只打开配置宏：

- **事件**：`ui_event` 位 0/1/2 分别表示心跳、按键、软件定时器。LCD 用 `RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR` 等待并消费通知。事件位合并重复通知，不承担逐条数据保存。
- **消息队列**：心跳发送 32 位样本，LCD 接收并统计。32 B 静态池包含每条 4 B 消息头和 4 B 数据，最多 4 条。满队列时本演示记录丢弃；内核同时支持阻塞收发和超时。
- **互斥量**：`counter_lock` 保护应用计数，USB 输出缓冲和堆分配也分别使用互斥量。保留官方所有者检查、递归获取和优先级继承；互斥量不能用于 ISR。
- **软件定时器**：默认 200 ms，回调在独立 `timer` 线程执行，增加计数并发事件。Timer0 IRQ 推进 tick 和硬定时器，软件回调不在 IRQ 中执行。回调保持简短，不阻塞等待，不做 LCD 绘图。
- **动态线程**：心跳通过 `rt_thread_create()` 创建，控制块从 2 KB XDATA 堆申请，硬件栈从 EDATA 专用池申请。入口返回后走内核退出逻辑，由 idle 调用 cleanup、回收控制块和栈槽。`dyn stop` 协作退出，避免删除持有互斥量的线程。

上述 IPC 和定时器的静态初始化、动态创建/删除 API 均已启用，底层使用官方内核实现；动态 IPC 对象从同一 XDATA 堆申请。信号量和内核硬定时器继续保留。

## 动态线程示例与限制

`app/main.c` 的 `demo_dynamic_start()` 使用：

```c
rt_thread_t thread;
/* 初始化系统定时器、调度器和 XDATA 堆后调用。 */
thread = rt_thread_create("beat", heartbeat_thread_entry,
                          RT_NULL, 512, 1, 5);
if (thread != RT_NULL)
{
    thread->cleanup = heartbeat_cleanup;
    rt_thread_startup(thread);
}
```

**当前只支持同时存在 1 个动态线程，申请栈大小为 128–512 B。** 心跳默认占用该槽；未退出并由 idle 回收前，再次创建返回 `RT_NULL`，失败的控制块申请会回滚。创建接口检查入口、优先级、栈大小和时间片。

不能将 XDATA 堆直接当线程栈：STC 的 SP 只支持本移植采用的低地址 EDATA 硬件栈。创建其他动态任务前，先 `dyn stop`，等 `dyn status` 显示 stopped/free，再在源码中调用创建接口。

按键和 LCD 仍使用 `rt_thread_init()` 和静态 EDATA 栈。控制块和栈必须在线程运行期间持续有效，不能使用 main 的临时局部数组。优先级数字越小越高；5 tick 时间片仅对同优先级就绪线程生效，周期由 delay/IPC 等待决定。

## 移植原理与内存

STC32G 的 `TRAP` 是 NOP。本移植在 `port/context.a51` 将普通 LCALL 的 2 B 返回地址扩展为与硬件中断一致的 4 B 栈帧。Timer0、USB IRQ 保存 R0–R31、DR56/DPX、PSW0、IE、DPS、DPTR1、P_SW2，统一恢复并执行 RETI；SP 通过 DR60 切换。

所有 C 源码保持 `FUNCTIONS(REENTRANT)`。XSMALL 数据指针为 4 B、near 函数指针为 2 B；C251 的 int/size_t 为 16 位、long 为 32 位。线程栈放在 `00:0000–00:0FFF` EDATA 并向高地址增长，控制块、堆、USB 和 MSH 文本缓存主要放在 XDATA。堆申请加入 16 位长度乘法/对齐溢出保护。

Keil 统计：**DATA 8 B，EDATA 3813 B，XDATA 6349 B，常量 3990 B，代码 33459 B**。EDATA 最高已用地址约 `0EECH`，余量约 275 B；XDATA 余量约 1843 B。2 KB 堆包含在上述 XDATA 中，也包含分配器元数据，实际可用量以 `mem` 为准。

| 线程/用途 | 优先级 | 栈 |
|---|---:|---:|
| beat，动态 | 1 | 512 B |
| key，静态 | 2 | 384 B |
| timer，软件定时器 | 3 | 384 B |
| lcd，横屏 GUI | 4 | 640 B |
| tshell，静态 FinSH | 5 | 768 B |
| tidle0 | 7 | 384 B |
| 启动阶段 | — | 384 B |

默认 6 个线程，停止心跳后为 5 个。为容纳定时器线程，部分栈较基础版缩小，**需在板上反复执行命令、补全/历史、GUI 切页及 USB 重连，确认栈余量**。切换线程时检查栈哨兵；C251 异常直接记录 fault 并停机，避免调度/IRQ 上下文等待控制台互斥量。`ps` 是高水位估计，不是瞬时 SP；打印前复制线程快照，避免动态回收导致悬空引用。

Timer0 使用 12T 自动重装：35 MHz / 12 / 29166 ≈ 100 Hz；USB 独立 IRC48M。USB RX/TX 各 256 B，寄存器访问使用短 EA 临界区。TX 满只阻塞发送线程并有超时，64 B 整包补 ZLP；重连通过 Ctrl-C 丢弃上次未完成输入。`rt_kprintf` 用互斥量保护格式化缓冲，ISR 内输出被丢弃。

未启用 RT device、文件系统、网络协议栈或动态模块，是适配 12 KB RAM 的 Nano 演示。

## 编译与测试

在工程目录中运行：

```powershell
.\tools\build.ps1
.\tools\test.ps1 -Python python
python .\tools\package.py
```

编译默认使用 `D:\Keil_v5\UV4\UV4.exe`，可用 `-Keil` 指定。Native 测试使用 VS2022 x86，可修改 `tools/test-host.cmd` 的 vcvars32 路径。

- `tests/kernel_host.c`：实际内核/FinSH/MSH 的调度、硬定时器、信号量、编辑和新增命令。
- `tests/components_host.c`：事件 OR/AND/CLEAR/唤醒/超时；队列 FIFO/urgent/满队列/阻塞收发；互斥量递归/所有者/优先级继承；软件定时器周期/单次/停止/回绕；堆耗尽/重分配/溢出；50 次动态线程回收、正常入口退出、动态 IPC 创建删除无泄漏。
- `tests/usb_console_host.c`：真实 CDC 桥接与模拟 USB，检查流控、重连、CRLF、ZLP、超时、未连接时 LCD 捕获。
- `tests/gui_host.c`：实际缓存和 GUI，检查横屏布局边界、分页按键、滚动/退格/常用 ANSI/换行、只更新变化行。
- `tests/context_machine.py`：执行 HEX 中的实际上下文切换指令，750 个随机场景，检查寄存器、栈帧、Timer0/USB、XDATA 指针与栈地址。
- `tests/usb_descriptors.py`：CDC 描述符、接口、端点和长度。

完整 rebuild：**0 errors，37 个 C47/C138 未使用参数或 `(void)` 无效果提示**，没有指针类型不符、未定义符号或 RAM 溢出警告。Native 队列有两处 32 位 size_t 到官方 16 位容量字段的 C4267 提示，本演示固定 4 B/4 条，不发生截断。软件测试不执行整块 MCU，不能证明硬件栈充足。

烧录后只读检查：

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\test_usb_board.ps1 -Port COM7
```

手动验证：`page msh` 后执行 `help` / `ps` / `ipc` / `mem`；`timer off` / `timer 100` 观察计数；`dyn stop` 后等约 1 s 和 idle 回收，查看 `dyn status`、`mem`，再 `dyn start`，重复确认堆占用恢复。左右键/P3.2 切页后返回 MSH 应保留最新输出。

`rt_port_fault`：1=栈地址，2/3=切换或退出异常，4=初始化失败，5=调度器意外返回，6=栈哨兵/边界异常。烧录黑屏或无 COM 时先确认 HEX、35 MHz 和 4 B 中断帧。

## 文件与来源

- `app/main.c`：IPC、静态/动态线程、软件定时器示例。
- `app/gui.c`、`app/gui_console.c`：横屏页面、MSH 文本环形缓存。
- `app/commands.c`：显式命令表，C251 不使用 linker section 自动收集。
- `port/dynamic_stack.c`：EDATA 动态栈池。
- `kernel/src/mem.c`：官方 small-memory 分配器。
- `port/`：C251 启动、上下文切换与链表适配。
- `bsp/usb/`、`bsp/usb_console.c`：原生 USB 驱动与控制台桥接。
- `THIRD_PARTY.md`、`kernel/LICENSE` 和源码头：来源、许可。

应用、配置与板级中文注释采用 GBK，文档采用 UTF-8，第三方许可保留。
