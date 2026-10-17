# Tunuino connector pinout baseline: Proteus F4

## Hardware decision

The Tunuino ECU uses **both Proteus F4 external connectors** as the reference for its connector-side signal assignment. The MCU remains **STM32F429ZIT6 (LQFP144)**.

This decision fixes the connector family and the intended external wiring reference; it does **not** establish verified connector pin numbers or MCU GPIO mappings.

## Mapping rules

1. Obtain the exact Proteus F4 connector pinout and identify the board revision and connector orientation (mating-face versus PCB view).
2. Document every pin on connector 1 and connector 2, including power, grounds, reserved positions and unused pins.
3. Preserve the external connector signal assignment wherever electrically compatible with the Tunuino hardware.
4. Independently assign STM32F429 GPIO and alternate functions using the MCU datasheet and verified PCB schematic.
5. Support up to eight high-impedance saturated injector channels and eight logic-level outputs for smart ignition coils, subject to driver validation.
6. Include crank/cam inputs, analog sensors, communications and external LSU 4.9 wideband CAN input as permitted by the verified connector assignment.
7. Check current limits, input protection, grounding, driver fault states and power-up behaviour before any output is enabled.

## Verification status

**Connector pin numbers: pending authoritative source.**

**MCU-to-connector netlist: pending schematic review.**

**Injector/ignition outputs: disabled pending hardware validation.**

Do not invent connector pin numbers from unrelated ECU variants or infer them from the MCU package. A cross-compiling firmware binary does not validate connector wiring.
