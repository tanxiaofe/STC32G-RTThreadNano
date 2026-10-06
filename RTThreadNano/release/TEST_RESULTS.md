# Verification result — IPC and landscape GUI update

Verified hardware: Laoliang open-source oscilloscope V1.1 (STC32G12K128).

Target: STC32G12K128, C251 Source Mode / XSMALL / four-byte IRQ frame, 35 MHz.

- Keil rebuild: 0 errors, 37 C47/C138 unused-parameter/no-effect warnings.
- DATA 8 B, EDATA 3813 B, XDATA 6349 B, constants 3990 B, code 33459 B.
- Actual native kernel/FinSH/MSH tests: PASS, including new ipc/timer/dyn/mem/page commands.
- Event OR/AND/CLEAR, blocking wakeup and timeout: PASS.
- Message queue FIFO/urgent/full/blocked send and receive/timeouts: PASS.
- Mutex recursive ownership and priority inheritance/restoration: PASS.
- Software timer periodic/oneshot/stop/control/tick wrap and non-IRQ dispatch: PASS.
- Small-memory allocator exhaustion/calloc/realloc/overflow checks: PASS.
- Dynamic thread allocation rollback, 50 delete/reuse cycles and natural exit/idle reclamation: PASS.
- Heap-backed event/mutex/queue/timer create/use/delete without leaks: PASS.
- Native USB CDC controller tests, including LCD capture without a host or on TX timeout: PASS.
- Actual GUI/text-cache logic: landscape bounds, page controls, ring scrolling, CRLF/backspace/ANSI and changed-line redraw: PASS.
- Linked context opcodes: 750 randomized scenarios plus Timer0/USB/XDATA-pointer cases: PASS.
- HEX checksums, all six EDATA thread stacks and XDATA heap bounds: PASS.
- CDC descriptors: PASS.

The maintainer has flashed and verified the current expanded firmware on the development board, including IPC, dynamic threads and the landscape GUI with its MSH page. Native tests mock CPU switching or USB peripherals; the instruction model is not a complete STC simulator. Reduced stack sizes require hardware watermark checks under command, GUI and USB reconnect load.

Repeat with `tools/build.ps1` and `tools/test.ps1`. After flashing, use `tests/test_usb_board.ps1` and the manual component checks in README.
