# STC32G12K128 — RT-Thread Nano + USB CDC FinSH

这是为当前 STC32G12K128 / TFT 480×320 V2 开发板新建的独立工程，原有示波器、游戏、模型显示功能没有加入本工程。

内核来自你提供的 `rtthread-nano-master.zip`，版本 **RT-Thread Nano 4.1.1**。FinSH 使用该压缩包中的官方 `components/finsh` 源码，启用 **MSH 命令行模式**。支持原生 USB CDC 虚拟串口；没有使用 USB 转 UART。

**验证状态：C251 完整编译、电脑端内核/FinSH/CDC 测试、编译后汇编指令检查已通过。尚未在开发板上烧录验证启动、USB 枚举或实时栈余量。**

## 使用

1. 用 Keil C251 打开 `RTThreadNano.uvproj`，或直接烧录 `release/RTThreadNano-CDC-FinSH.hex`。
2. STC-ISP 下载设置使用 **35 MHz**。工程使用 **251 Source Mode、XSMALL、4 字节中断栈帧**；不要修改为 8051 模式或 2 字节中断栈帧。
3. 烧录结束后复位，通过板上的 MCU 原生 USB 数据口连接电脑。Windows 10/11 的 CDC ACM 驱动匹配依据见下方 Microsoft 文档；本工程设备描述符已设为 `02/02/01`。
4. 在串口终端打开新出现的 COM 口，115200、8N1、无硬件流控，最好启用 DTR。USB 实际传输不依赖这个名义波特率。关闭终端的本地回显，固件会回显输入。
5. 看到 `msh >` 后输入 `help`。没有提示符时按 Enter 或 Ctrl-C；重新打开串口也会刷新提示符。

也可使用附带的简单 Windows 终端：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\usb_terminal.ps1 -Port COM7
```

未指定 `-Port` 时，脚本仅在发现唯一 `VID_34BF&PID_FF02` 的串口时自动选取。输入 `exit` 关闭电脑终端。该脚本使用逐行输入；若需要方向键历史和 Tab 补全，请使用支持 VT/ANSI 的串口终端。

## 命令

| 命令 | 功能 |
|---|---|
| `help` | 命令及说明 |
| `version` | RT-Thread 与开发板信息 |
| `ps` | 5 个线程的优先级、状态、栈使用高水位 |
| `tick` | 100 Hz 系统 tick |
| `stat` | 心跳、按键、屏幕更新次数及心跳周期 |
| `beat 200` | 心跳改为 200 ms；范围 100–1000 ms，步长 10 ms |
| `reset` | 清零演示计数，不复位 MCU |
| `clear` | 清除 ANSI 终端画面 |
| `echo "hello STC"` | 回显参数，支持引号 |

支持退格、左右光标、Tab 补全、上下键历史（2 行）、Ctrl-C 取消输入。输入行达到 64 字节会整行丢弃，不执行剩余片段。MSH 不是 FinSH 的 C 表达式解释器；本配置不包含 C 表达式求值、文件系统和动态模块。

## 屏幕和按钮

竖屏显示内核版本、tick、心跳、按键和屏幕更新次数。只有 LCD 线程操作屏幕，其他线程通过信号量通知它更新。

- P7.4 / 上：缩短心跳周期。
- P7.3 / 下：延长心跳周期。
- P7.0 / 中：清零心跳与按键计数。
- P7.2、P7.1、P3.2：产生按键事件，显示按键掩码。

线程优先级：beat=1、key=2、lcd=4、tshell=5、tidle0=7。数值越小优先级越高；心跳和按键可抢占屏幕/控制台线程。

## 移植原理和限制

STC32G 的 `TRAP` 指令是 NOP，不能照搬 Intel C251 的软件中断切换方案。本移植在 `port/context.a51` 中把普通 LCALL 的 2 字节返回地址扩展为 `PSW1 + bank + PCL + PCH`，与硬件中断的 4 字节返回栈帧统一。

Timer0 和 USB 中断都先保存现场，再调用 C 处理函数，最终统一恢复并执行 RETI。保存 R0–R31、DR56/DPX、PSW0、IE、DPS、DPTR1、P_SW2。SP 通过 DR60 切换；PSW1 和 PC 由统一的返回栈帧恢复。软件切换必须在调度器已经关闭 EA 的情况下调用。

**所有 C 源码必须保持 `FUNCTIONS(REENTRANT)`。** C251 V2+ 的可重入函数使用硬件栈，普通函数的静态局部变量/overlay 不能用于抢占多线程。C251 没有 inline，本移植将链表辅助函数集中到 `port/list.c`，避免每个模块重复生成静态函数。

XSMALL 的普通数据指针是 4 字节、near 函数指针是 2 字节。数据类型按 C251 的 int=16 位、long=32 位适配；格式化参数和 ffs32 同步修正。`rt_tick_get()` 加入临界区，避免 32 位 tick 读取撕裂。

线程硬件栈只允许放在 `00:0000–00:0FFF` 的 **EDATA**，并向高地址增长。即便 XDATA 还有空闲，也不能直接拿来当线程硬件栈。TCB、FinSH 编辑缓冲区、USB 收发缓冲区和日志缓冲区放在 XDATA；启动代码清零这两个 RAM 区。`rt_thread_init()` 拒绝不支持的栈地址或大小。

配置采用静态线程，不启用 heap、动态创建线程、软件定时器线程、RT device 框架或文件系统。保留内核硬定时器、信号量和互斥量。控制台通过 `rt_hw_console_getchar/output` 接入，这是无 device 框架的 Nano 控制台接口。

Timer0 使用 12T、16 位自动重装：35 MHz / 12 / 29166 ≈ 100 Hz。USB 使用独立 IRC48M 时钟。USB IRQ 入口在 FF:00CB；Timer0 在 FF:000B。

USB RX/TX 环形缓冲区各 256 字节。USB 寄存器访问使用短 EA 临界区，防止 Timer0 抢占后留下锁定的 USB 状态。TX 满时只阻塞发送线程，并有超时；64 字节整包结束时补 ZLP。总线复位清空收发索引；DTR 重开/重连通过 Ctrl-C 清除未完成输入，避免执行上一连接残留的半条命令。`rt_kprintf` 使用互斥量保护共享格式化缓冲区；本配置丢弃 ISR 内的控制台输出。

## 内存与栈

当前 Keil 统计：DATA 8 B（寄存器 bank 0 保留）、EDATA/HDATA 3893 B、XDATA 1587 B、常量 2848 B、代码 21923 B。EDATA 的最高已用地址为 0F3CH，剩余约 195 B；增加线程或功能前必须重新检查 map 和实际 `ps` 的栈余量。

| 用途 | 栈大小 |
|---|---:|
| beat | 512 B |
| key | 512 B |
| lcd | 768 B |
| tshell | 1024 B |
| tidle0 | 384 B |
| 启动阶段 | 384 B |

`ps` 中的 used 是基于填充字符扫描的高水位估计，不是精确的瞬时 SP。硬件栈使用量需要在实际板上反复执行 help、ps、Tab、方向键、按键并插拔 USB 后确认。

## 编译和测试

```powershell
.\tools\build.ps1
.\tools\test.ps1 -Python python
```

编译脚本默认 Keil 路径为 `D:\Keil_v5\UV4\UV4.exe`，可用 `-Keil` 指定安装位置。Native 测试使用当前电脑的 Visual Studio 2022 x86 工具链；可调整 `tools/test-host.cmd` 的 vcvars32 路径。

- `tests/kernel_host.c`：运行原始内核与实际 FinSH/MSH 代码，检查优先级、时间片、延时唤醒、信号量、硬定时器、tick 回绕、ffs32、命令参数、补全、历史、Ctrl-C、长行丢弃。
- `tests/usb_console_host.c`：用模拟 USB 控制器运行实际 CDC 控制台适配代码，检查缓冲区回绕、流控、重连、CRLF、TX 超时和 64 B/ZLP。
- `tests/context_machine.py`：读取实际 HEX 和 map，在一个有限 C251 指令模型中执行已链接的上下文切换机器码，检查所有寄存器、SP/PC/PSW、Timer0/USB 入口及 XDATA TCB 指针。它不模拟整个 MCU、内核 C 代码或真实 USB 控制器。
- `tests/usb_descriptors.py`：检查 CDC 描述符、接口、端点和长度。

完整 Keil rebuild 为 **0 errors**；有 27 个上游/演示未使用参数或 `(void)` 无效果提示（C47/C138），没有截断、未定义符号或 RAM 溢出警告。未调用内核 API 的 L16 提示在工程中关闭。

烧录后可执行板上只读检查：

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\test_usb_board.ps1 -Port COM7
```

此脚本只验证枚举后的串口命令、线程列表及 tick 推进，不替代长时间硬件测试。当前尚未运行此板上脚本。

若屏幕停在黑屏/计数不动或没有 COM 口，先确认烧录的是本工程 HEX、35 MHz 设置和 4 字节中断帧；再查看 `rt_port_fault`。错误码：1=栈地址、2/3=切换或线程退出异常、4=初始化失败、5=调度器意外返回、6=演示线程栈尾损坏。

## 文件与来源

- `RTThreadNano.uvproj`：独立 Keil 工程。
- `release/RTThreadNano-CDC-FinSH.hex`：烧录文件。
- `port/`：C251 上下文切换、启动和链表辅助函数。
- `bsp/usb/`：STC 原生 USB 驱动；`bsp/usb_console.c`：RT 控制台桥接。
- `components/finsh/`：原版 FinSH/MSH 及 C251 适配。
- `app/commands.c`：显式命令表。C251 不支持原版 linker section 收集，新命令在这里注册。
- `THIRD_PARTY.md`、`kernel/LICENSE`：来源及许可。

参考：[STC32G 官方手册](https://www.stcmicro.com/datasheet/stc32g-cn.pdf)（附录 P 的 TRAP/RETI 及寄存器说明）、[Keil C251 编译选项](https://www.keil.com/support/man/docs/uv4cl/uv4cl_dg_c251.htm)、[Microsoft USB CDC/Usbser 文档](https://learn.microsoft.com/zh-cn/windows-hardware/drivers/usbcon/usb-driver-installation-based-on-compatible-ids)。

## 源码中文注释

应用线程、板级初始化、LCD 文本、USB CDC 控制台、FinSH 命令表、CPU 上下文切换和内核配置已添加中文注释。上述源码采用 GBK 编码，便于 Keil C251 显示；说明文档采用 UTF-8。注释不改变程序逻辑，第三方源码许可声明保留。

注释更新已通过完整编译和现有软件验证。对比烧录内容，仅版本信息中的编译时间发生变化，代码与其余数据保持一致；尚未进行开发板实测。
