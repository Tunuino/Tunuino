# TIM2 free-running counter (first hardware step)

TIM2 on STM32F407 is a 32-bit general-purpose timer. This module prepares a 1 MHz counter (1 microsecond/tick) from a **verified** TIM2 peripheral timer clock.

With the current firmware boot path using 16 MHz HSI and APB1 prescaler /1, TIM2 clock is nominally 16 MHz and PSC = 15.

The driver deliberately does not configure any GPIO alternate-function mappings, input-capture channels, DMA or interrupts. It also is **not yet called from main**, pending board integration.

Before enabling real CKP/CMP capture:
- Verify MCU pin mapping, electrical input protection and VR/Hall conditioner.
- Choose TIM2 CH1/CH2 pins and verify alternate function mapping.
- Implement CCR capture interrupts and account for counter rollover and interrupt latency.
- Test on real hardware with a signal generator and oscilloscope.
- Never enable injector or coil drivers based on this counter alone.
