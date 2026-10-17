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
- Pin assignments use the Tunuino STM32F429 board configuration; connector wiring, actual MCU alternate functions, and the CAN transceiver must be verified before enabling GPIO or sending frames.
- No injector or ignition GPIO is assigned or enabled.

## Connector-specific development baseline

Tunuino is now planned around **Proteus F4 J102 and J103 only**. The source-derived 70-pin signal table and candidate eight-channel injector/ignition assignments are in [docs/proteus-f4-connectors.md](../../../docs/proteus-f4-connectors.md). J101 is not part of the Tunuino external connector plan.

The source schematic does not yet prove how power, grounds, crank/cam and CAN are routed through the selected two connectors. Before implementing live peripheral GPIO mux or enabling power drivers, export the complete KiCad netlist and validate those connections and STM32F429 alternate functions. In particular, the current USART2 and CAN1 MCU pin declarations are **not** evidence that these signals reach J102/J103.

The cross-compiled `tunuino.bin` is a **bench-development image only**. It is not yet a tested ECU control firmware, and the current serial interface remains intentionally disconnected.
