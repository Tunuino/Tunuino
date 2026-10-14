# Tunuino Console firmware transmitter

The firmware provides `console_tx.h/.c` to emit protocol v1 HELLO and telemetry frames through a caller-provided callback.

- HELLO payload is `54 4E 01` (TN + protocol version).
- Telemetry is the seven-field 14-byte payload documented in `console-protocol-v1.md`.
- The callback must accept the **entire frame atomically** or report failure. Partial writes are not retried because retransmitting part of a frame without transport state can corrupt the stream.
- Frame construction is stack-only and does not allocate heap memory.
- No USB, UART, DMA, interrupt, pin mapping or scheduling is implemented.
- No sensor data is sampled by this module. The caller must supply measured and validated values.
- No injector/ignition outputs or calibration writes are enabled.

Next step: implement a verified transport and a bounded TX queue, then integrate with real sensor acquisition and explicit data-validity flags.
