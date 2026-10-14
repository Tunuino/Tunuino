# Tunuino STM32F429ZIT6 platform

Target: STM32F429ZIT6, Cortex-M4F, LQFP144.

This directory contains the first **host-testable board state interface**, not a functioning STM32 hardware abstraction layer.

## Next hardware tasks

1. Select and license a compatible CMSIS/device-header and startup implementation.
2. Confirm clock source, regulator, SWD, reset, boot configuration and watchdog.
3. Validate connector-to-GPIO pin mapping against the LQFP144 package and alternate functions.
4. Implement safe startup GPIO defaults and hardware output-enable interlock.
5. Configure TIM capture/compare for crank/cam and event scheduling.
6. Configure CAN1 with a real transceiver for the external LSU 4.9 controller.
7. Add cross-compilation and target-level tests.

**Do not connect injectors, coils or an engine to this code.** Startup, clock and read-only transport scaffolding exist, but GPIO, CAN and engine power outputs are not enabled.

## Proteus F4 peripheral reference (not electrically activated)

- Console: USART2 TX PD5, RX PD6, AF7.
- Wideband external controller: CAN1 RX PD0, TX PD1, AF9.
- Pin assignments follow the rusEFI Proteus F4 board configuration; connector wiring and CAN transceiver must be verified before enabling GPIO alternate functions or sending frames.
- No injector or ignition GPIO is assigned or enabled.
