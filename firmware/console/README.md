# Tunuino firmware console

This directory owns the ECU-side communication architecture. It is distinct
from the browser-based Tunuino Console application at the repository root.

## Current implementation

The transport-independent framing, tune read and UART pump are currently in
`firmware/core/`:

- `console_protocol.c`: framing and message validation.
- `console_tune_read.c`, `console_tune_request.c`, `console_tune_rx.c`,
  `console_tune_link.c`: tune read protocol.
- `console_tx_queue.c`, `console_tx.c`, `console_uart_pump.c`: buffered output.

STM32F429-specific peripheral access remains in
`firmware/platform/stm32f429/usart_console.c`. The UART peripheral is
intentionally left disconnected pending verified board wiring.

## Architecture

Browser application (`/console`)
  <-> Tunuino framed protocol (`/firmware/core`)
  <-> ECU console transport (`/firmware/console`)
  <-> verified STM32 peripheral driver (`/firmware/platform/stm32f429`)

Planned responsibilities: USB CDC device transport, UART RX/TX, command
dispatch, telemetry subscriptions, fault diagnostics and tune operations.
Bootloader communication is a separate recovery protocol.

**Not implemented:** functional USB CDC device, end-to-end connection to
hardware, live tuning writes, ECU flash programming. No ignition or injector
outputs should be enabled as a side effect of console development.

The code is organized under the Tunuino identity and remains independent.
