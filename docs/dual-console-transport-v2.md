# Dual protocol transport v2

The Tunuino Console side uses type-3 partial telemetry and the existing HELLO handshake. The experimental TunerStudio side queues read-only replies independently, with atomic whole-frame insertion into a 256-byte queue. One byte per endpoint is transmitted per step, if its transport reports ready.

Neither side blocks waiting for the other. The producer and consumer must run in one serialized context; the queue is not interrupt-safe. A missing UART callback disables setup. Identical non-null callback contexts are rejected as a basic guard, but do not prove distinct physical UARTs.

No STM32 UART, pin, IRQ or DMA is configured. The TunerStudio INI remains a prototype; real application interoperability is not yet established. Both interfaces are read-only.
