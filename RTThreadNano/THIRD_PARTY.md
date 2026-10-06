# Third-party sources

- `kernel/` and `components/finsh/`: RT-Thread Nano 4.1.1. Apache-2.0; see `kernel/LICENSE` and file headers. FinSH uses the canonical `rt-thread/components/finsh` source, not the STM32 BSP copy.
- `bsp/usb/`: STC MCU Limited USB CDC demo, reused from the existing USBVideo project. Original author notices remain intact. The project states that it uses STC MCU Limited's USB routines.
- `bsp/TFT480x320-V2.c`, panel headers and font: original board LCD support, reused from the existing project. Original panel initialization/timings retained.
- `port/startup.a51`: Keil C251 START251.A51 template from the user's installed toolchain, with EDATA/XDATA zero ranges and bootstrap stack adjusted. Keil copyright retained. This file is not relabeled as Apache-2.0.

The new CPU port, board adapters, demo, console integration and tests identify their own source license where appropriate. No third-party binaries are bundled except the compiled project firmware.

The enabled small-memory allocator `kernel/src/mem.c` also retains the original lwIP/Swedish Institute of Computer Science BSD-style notice and conditions in its header. Include that file with source and binary distributions.
