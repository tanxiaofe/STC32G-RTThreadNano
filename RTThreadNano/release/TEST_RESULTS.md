# Verification result

Target: STC32G12K128, C251 Source Mode / XSMALL / INTR=1, 35 MHz.

- Clean Keil rebuild: 0 errors, 27 C47/C138 unused-parameter/no-effect warnings.
- DATA 8 B, EDATA/HDATA 3893 B, XDATA 1587 B, constants 2848 B, code 21923 B.
- Native 32-bit kernel tests: PASS (priority, time slice, yield, delay/preemption, semaphore, hard timer, tick wrap, ffs32).
- Real native FinSH/MSH source: PASS (commands, quoted arguments, editing, Tab, history, Ctrl-C, overflow discard).
- Real console adapter with fake USB registers: PASS (reconnect, CRLF, wrap, OUT flow control, 64-byte/ZLP, backpressure, timeout, atomic access).
- Linked context machine-code subset model: PASS (750 randomized scenarios plus Timer0/USB/XDATA-pointer cases).
- HEX checksum/flash bounds, five EDATA thread stacks, 4-byte interrupt frame: PASS.
- USB CDC descriptors: PASS (02/02/01 class, interfaces, endpoints, UTF16 product string).

No board flash, real USB enumeration, hardware execution or runtime stack-watermark measurement has been performed. The ISA model is a bounded instruction model, not a full STC simulator.

Repeat with tools/build.ps1 and tools/test.ps1. Hardware smoke test: tests/test_usb_board.ps1.
