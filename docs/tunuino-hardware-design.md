# Tunuino ECU: electrical schematic and PCB design specification

## Revision status

**DESIGN SPECIFICATION ONLY**. Not an electrical schematic, KiCad PCB, released netlist or manufacturing package. The Proteus F4 is the engineering reference, not the Tunuino board to be manufactured.

## Fixed requirements

- MCU STM32F429ZIT6, LQFP144, 3.3 V I/O domain.
- Two external automotive connectors only: J102 and J103, 35 positions each. [Pin allocation](proteus-f4-connectors.md) is the target interface; positions designated REWORK need newly designed circuits and PCB routing.
- Eight high-impedance injectors with correctly rated saturated low-side drivers, hardware OFF default and flyback energy management.
- Eight smart coils driven by verified logic-level interfaces with hardware OFF default and reset/fault interlock.
- CKP and CMP protected VR/Hall-compatible frontends, appropriate differential/grounding design, feeding timer-capable MCU inputs.
- CAN1 wideband interface via automotive transceiver, ESD/transient protection, selective termination and validated bus topology.
- Analogue sensor inputs, thermistor pullups, knock frontend, protected 5 V sensor rails, sensor grounds and independent analogue reference strategy.
- USB service/programming plus SWD accessible on-board, not counted as an extra automotive harness connector.

## Schematic sheet partition

| Sheet | Mandatory circuitry and design verification |
|---|---|
| 01 Power | Reverse polarity, load dump/transients, fuse coordination, input filters, buck/LDO rails, sequencing, watchdog and brownout behaviour |
| 02 MCU | STM32F429ZIT6, decoupling per supply pin, VDDA/VREF, reset, BOOT, clocks, SWD and test points |
| 03 Connectors | J102/J103 all 70 numbered contacts, mating-face pin orientation, net classes, power/ground current budget |
| 04 Injector drivers | 8 independently controlled saturated channels, short/open diagnostics as feasible, flyback, thermal/current limits, hardware inhibit |
| 05 Ignition | 8 smart-coil logic interfaces, voltage/polarity compatibility, output resistors/protection and hardware inhibit |
| 06 Triggers | CKP/CMP VR/Hall frontends, input filtering, hysteresis, differential sensing and timer inputs |
| 07 Sensors | 11 analogue voltage inputs, 4 temperature inputs, knock channels, filtering, clamping and sensor 5 V supplies |
| 08 Communications | CAN transceiver, ESD/EMC protection, termination selection, USB data/power protection and service connector |

## Interface and MCU allocation rules

1. The external connector assignments are defined in the 70-pin target table. Do not equate external pins with STM32 GPIO pins.
2. Allocate STM32 GPIO/ADC/timer/CAN/USB functions only after selecting and checking actual conditioning and driver circuits. Produce a separate mapping of **connector pin -> circuit net -> driver/conditioner -> MCU pad/AF**.
3. Keep J102 power-ground and switched supply allocations provisional until the complete current budget and contact rating are calculated; add/redistribute contacts if needed.
4. For J103 CKP/CMP, connector contacts carry sensor-side signals. Do not feed raw VR waveforms directly into MCU pins.
5. All high-current outputs must be physically disabled during MCU reset, bootloader, power failure and watchdog timeout, independently of software.
6. Separate noisy driver return currents from sensor analogue grounds with an engineered return topology; do not assume a generic ground plane alone is sufficient.

## Release gates for manufacturing

- KiCad schematic with all hierarchical sheets, symbols, footprints, part numbers and complete annotated netlist.
- ERC clean or reviewed and justified exceptions; checked component datasheets and footprints.
- PCB placement, copper widths, thermal reliefs, creepage/clearance, connector mechanical fit, mounting and enclosure tolerances.
- DRC clean or justified exceptions; reviewed 3D model and connector orientation.
- Electrical and thermal review for load dump, ESD, reverse battery, cranking, injector flyback, smart-coil interface and CAN.
- Gerber + Excellon drill + fabrication drawings + BOM + pick-and-place files generated **from the final KiCad board**.
- Independent bench validation of power, reset and outputs-safe state before connecting any engine hardware.

**No fabricated board or validated production outputs exist at this stage.**
