# Tunuino STM32F407ZGT6 platform

Target: STM32F407ZGT6, Cortex-M4F, LQFP144.

This directory contains the first **host-testable board state interface**, not a functioning STM32 hardware abstraction layer.

## Next hardware tasks

1. Select and license a compatible CMSIS/device-header and startup implementation.
2. Confirm clock source, regulator, SWD, reset, boot configuration and watchdog.
3. Validate connector-to-GPIO pin mapping against the LQFP144 package and alternate functions.
4. Implement safe startup GPIO defaults and hardware output-enable interlock.
5. Configure TIM capture/compare for crank/cam and event scheduling.
6. Configure CAN1 with a real transceiver for the external LSU 4.9 controller.
7. Add cross-compilation and target-level tests.

**Do not connect injectors, coils or an engine to this code.** No GPIO, clocks, timers, CAN or startup code are implemented yet.
