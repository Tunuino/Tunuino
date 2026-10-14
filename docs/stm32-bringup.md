# STM32F407ZGT6 bring-up checklist

## Safety prerequisite
A firmware variable cannot guarantee output safety during reset, brownout or a crashed MCU. The PCB must provide external hardware pull resistors, driver disable circuitry and a defined fail-safe power-up state. Never connect an engine until these are verified.

## Implementation order
1. Confirm power supplies, clock oscillator, reset circuit, BOOT0, SWD and watchdog.
2. Confirm all LQFP144 pins and STM32F407 alternate functions against the datasheet.
3. Choose compatible CMSIS device files, startup assembly, linker script and clock configuration; preserve third-party licenses.
4. Configure all output-related pins to electrically safe states before enabling peripheral clocks/drivers.
5. Initialize system clock and SysTick, then timers, ADC, UART/USB and CAN1.
6. Add a hardware watchdog and fault handling.
7. Test with a debugger and oscilloscope, with output drivers physically disconnected.

The current startup_contract module is host-testable state-machine logic, **not** a flashable firmware or real MCU startup implementation.
