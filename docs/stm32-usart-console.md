# STM32F407 USART2 Console driver (disarmed)

This initial USART2 driver computes BRR for oversampling-by-16 and can configure the baud divisor while keeping the peripheral disabled (UE=0, TE=0, RE=0).

For current 16 MHz APB1 clock, 115200 baud gives BRR=139. For 42 MHz APB1 clock, BRR=365. The caller must verify the actual peripheral clock before initialization.

No GPIO alternate functions, NVIC interrupts, DMA, TX or RX are enabled. This code is **not called by main** and does not establish communication with the Console.

Before hardware use, confirm PCB connector routing, USART2 TX/RX pins, voltage levels, signal integrity and serial adapter isolation as needed. Integrate only after board-level review.

Note: the abstract UART pump requires an atomic ready/send sequence in one serialized execution context. The current `send_byte` rechecks TX readiness; it is only a placeholder and not an IRQ-safe transmitter.
