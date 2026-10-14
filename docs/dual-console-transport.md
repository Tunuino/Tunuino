# Dedicated Console and TunerStudio transports

The experimental `dual_console_transport` adapter holds independent protocol services. Tunuino Console has its framed transmit queue; TunerStudio uses a separate synchronous read-only response callback. Incoming TunerStudio command bytes are never routed into the Console frame parser.

Both endpoint callbacks must be present. The adapter rejects an obviously identical non-null context pointer, but **pointer inequality does not prove physically distinct UARTs**. Board integration must assign verified separate peripherals, DMA/IRQ resources and pins. TunerStudio's synchronous write callback must provide all-or-nothing buffering; otherwise the response may be truncated. No physical UART is enabled.

The adapter currently supports legacy full Console telemetry. Type-3 partial telemetry uses the separate `console_partial_service` and will need a unified service selection before integration.

TunerStudio protocol compatibility remains experimental and has not been verified against the application.
