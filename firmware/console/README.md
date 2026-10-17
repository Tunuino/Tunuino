# Tunuino firmware console

This directory is the Tunuino firmware-side console integration location.
The complete desktop tuning application is not yet implemented in this repository.

## Reference-compatible module layout

| Reference console module | Tunuino responsibility | Current implementation |
| --- | --- | --- |
| `console_io` | Console transport I/O | `firmware/core/console_uart_pump.c`, `console_tx.c` |
| `eficonsole` | Command dispatch and framing | `firmware/core/console_protocol.c` |
| `status_loop` | Periodic status and telemetry | Firmware-side transport not yet connected |
| `usb_console` | USB device console | Not implemented |
| `connector_uart_dma` | UART/DMA transport | `firmware/platform/stm32f429/usart_console.c` (disabled) |
| `binary/tunerstudio` | Binary configuration protocol | `firmware/core/console_tune_*.c` (partial, independent protocol) |
| `binary/live_data` | Real-time data | Framing only; no hardware stream |
| `binary/tooth_logger` | Trigger tooth capture | Not implemented |
| `binary/trigger_scope` | Trigger diagnostics | Not implemented |
| `binary_log` and `binary_mlg_log` | Log storage/format | Not implemented |

The table is a **migration inventory**, not a claim of protocol compatibility.
The existing Tunuino modules remain in their original paths until their
include paths, build rules and tests can be migrated together. No duplicate
transport layer or speculative hardware driver is introduced.

## Integration rules

1. Preserve the reference directory/module responsibilities where applicable.
2. Keep the browser-based application separate from ECU firmware code.
3. Move working source files only with corresponding build and test changes.
4. Do not enable flash writes, injector/ignition outputs or USB peripherals
   before verified hardware and recovery support.
5. Maintain original project identity and respect upstream source licenses.

## USB compatibility target

Follow the Tunuino Console separation of USB serial diagnostics,
ECU signature and configuration protocol negotiation, and firmware flashing.
Do not treat the current experimental single-byte `S`/`A` protocol as wire-compatible
with other console implementations. USB CDC enumeration and the STM32F429 USB device stack are
not implemented yet. Desktop-side serial probing is preliminary only.

Tunuino-generated TunerStudio INI files must advertise a Tunuino signature
matching the firmware response. Other ECU INI files retain their own signatures.
When integrating upstream code, retain all legally required upstream license
and copyright notices.
