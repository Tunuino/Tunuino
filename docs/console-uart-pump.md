# Tunuino Console UART transmit pump

This module bridges the bounded Console TX queue to an abstract UART peripheral.

The caller supplies `ready(context)` and `send(byte, context)`. A byte is removed from the queue only after the UART reports readiness. The pump sends at most one byte per call and never waits for hardware.

**Concurrency contract:** both queue producer and pump consumer must execute in the same serialized context. The existing FIFO is not interrupt-safe; do not invoke the pump from an IRQ while the main loop writes to the queue without an explicit concurrency design.

**Hardware status:** no STM32 USART registers, GPIO AF pins, clocks, DMA or interrupts are configured. The selected USART and PCB pin mapping are still unverified. No power outputs are enabled.
