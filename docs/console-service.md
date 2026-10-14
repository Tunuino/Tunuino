# Tunuino Console service

The Console service combines protocol encoding, atomic-frame TX queue and nonblocking single-byte UART pump.

- HELLO is queued once per initialized service instance.
- Telemetry is queued at most once every 100 ms (10 Hz), **only** when the caller explicitly marks a supplied snapshot valid.
- One queued byte is transmitted per service step if the transport reports readiness.
- The service never samples sensors, fabricates readings, configures GPIO, enables UART, or controls engine outputs.
- All service calls and queue access must occur in a **single serialized execution context**. It is not interrupt-safe.
- A full queue rejects a complete frame; no partial frame is enqueued.

The STM32 `main.c` does not yet call this service because a verified UART transport and real sensor validity are unavailable. Host tests use an artificial sink to verify frame order and gating without implying hardware operation.
