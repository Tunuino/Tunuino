# STM32F429ZIT6 bring-up checklist

## Safety prerequisite

A firmware variable cannot guarantee output safety during reset, brownout or a crashed MCU. The PCB must provide external hardware pull resistors, driver disable circuitry and a defined fail-safe power-up state. Never connect injectors, ignition coils or an engine until these are verified.

## Implementation order

1. Confirm power supplies, clock oscillator, reset circuit, BOOT0, SWD and watchdog for STM32F429ZIT6 (LQFP144).
2. Confirm the package pinout and STM32F429 alternate functions against the manufacturer's datasheet and the actual PCB schematic.
3. Verify the selected CMSIS device headers, startup implementation, linker script and clock configuration; preserve applicable third-party licenses.
4. Configure all output-related pins to electrically safe states before enabling peripheral clocks or power drivers.
5. Initialize the system clock and time base, then timers, ADC, USART/USB and CAN1 as supported by the board.
6. Add hardware watchdog supervision, brownout and fault handling.
7. Validate crank/cam capture, CAN receive and console transport on a bench with power outputs physically disconnected.
8. Only after hardware review, implement and verify the injector and ignition driver interlocks with suitable test loads.

## Current implementation status

The firmware repository contains STM32F429 build, startup, clock and transport scaffolding, together with host-tested safety interfaces. A successful CI cross-compilation does not demonstrate verified GPIO multiplexing, CAN transceiver operation, timing accuracy or safe power-driver behaviour on real hardware.

Do not connect injectors, ignition coils or an engine to the present firmware.
