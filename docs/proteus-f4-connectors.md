# Tunuino STM32F429: J102 + J103 connector map

## Source and scope

Connector reference: [Tunuino/proteus](https://github.com/Tunuino/proteus/blob/master/proteus.kicad_sch), root schematic, revision v0.7 (title block). Connector J102: 35-pin Ampseal footprint `ampseal-35`, PN 776231-1. Connector J103: 35-pin Ampseal footprint `ampseal-35-2`, PN 776231-2. **J101 is excluded from the Tunuino connector plan.**

This table is extracted from **labels directly at the connector pin coordinates in the root schematic**. Entries without a direct label are deliberately unresolved: they can connect to other nets through wires, power symbols or sheet hierarchy. Do not treat them as unused or safe to reassign until the complete netlist is verified.

| Pin | J102 | J103 |
|---:|---|---|
| 1 | HS2 | A confirmar (ligação sem etiqueta direta) |
| 2 | HS1 | A confirmar (ligação sem etiqueta direta) |
| 3 | LS1 | A confirmar (ligação sem etiqueta direta) |
| 4 | LS3 | A confirmar (ligação sem etiqueta direta) |
| 5 | LS5 | A confirmar (ligação sem etiqueta direta) |
| 6 | LS6 | A confirmar (ligação sem etiqueta direta) |
| 7 | LS7 | A confirmar (ligação sem etiqueta direta) |
| 8 | LS9 | A confirmar (ligação sem etiqueta direta) |
| 9 | LS11 | 5V_SENSOR_1 |
| 10 | LS13 | 5V_SENSOR_2 |
| 11 | LS14 | A confirmar (ligação sem etiqueta direta) |
| 12 | LS15 | A confirmar (ligação sem etiqueta direta) |
| 13 | HS3 | AV1 |
| 14 | HS4 | AV3 |
| 15 | LS2 | AV5 |
| 16 | LS4 | AV7 |
| 17 | A confirmar (ligação sem etiqueta direta) | AV9 |
| 18 | A confirmar (ligação sem etiqueta direta) | AV11 |
| 19 | LS8 | AT1 |
| 20 | LS10 | AT3 |
| 21 | LS12 | 5V_SENSOR_1 |
| 22 | IGN3 | 5V_SENSOR_2 |
| 23 | LS16 | A confirmar (ligação sem etiqueta direta) |
| 24 | A confirmar (ligação sem etiqueta direta) | AV2 |
| 25 | IGN12 | AV4 |
| 26 | IGN11 | AV6 |
| 27 | IGN10 | AV8 |
| 28 | IGN9 | AV10 |
| 29 | IGN8 | A confirmar (ligação sem etiqueta direta) |
| 30 | IGN7 | AT2 |
| 31 | IGN6 | AT4 |
| 32 | IGN5 | 5V_SENSOR_1 |
| 33 | IGN4 | 5V_SENSOR_2 |
| 34 | IGN2 | KNOCK_1 |
| 35 | IGN1 | KNOCK_2 |

## Tunuino channel allocation (provisional)

- Injector drivers (high-impedance saturated): reserve eight suitable low-side channels, **LS1–LS8**, subject to driver current, flyback and PCB trace validation. The reference signal name alone does not prove electrical suitability.
- Smart ignition coils: reserve **IGN1–IGN8**, subject to verification of logic levels, driver topology, connector-side protection and safe reset states.
- Other low-side, high-side and ignition-labelled connector pins are not automatically assigned to Tunuino functions.
- J103 analogue inputs AV/AT, knock inputs and sensor supply nets require analogue-front-end and grounding verification before use.
- CAN1 and crank/cam assignments **are not established** by the direct labels of J102/J103. The third connector J101 is excluded, so these essential functions need a deliberate allocation and electrical design review rather than silently inheriting the three-connector Proteus design.
- STM32F429ZIT6 GPIO/alternate-function mapping remains **unassigned** until the exact two-connector PCB netlist is verified.

## Safety gates before firmware activation

1. Export and cross-check the complete KiCad netlist, including wires, power symbols and hierarchical connections, against connector numbering and mating-face orientation.
2. Verify all power and ground pins, protections, external drivers, sensor supply limits and MCU voltage domains.
3. Assign each external net to an actual STM32F429 pin or dedicated driver, checking timer/ADC/CAN/USB constraints.
4. Verify boot/reset and hardware output-disable behaviour before enabling injection or ignition.
5. Bench-test without injectors, coils or engine attached; do not use this table as an installation wiring harness diagram yet.

## Proposed two-connector integration plan

**Keep the verified connector labels fixed** while adapting the internal electronics to the STM32F429. The following is a design allocation, not a verified MCU netlist:

| ECU function | Connector pin(s) | Status / requirement |
|---|---|---|
| Injector 1–8 | J102: 3, 15, 4, 16, 5, 6, 7, 19 (LS1–LS8 respectively) | Candidate only; confirm saturated driver ratings, flyback and ordering |
| Smart ignition 1–8 | J102: 35, 34, 22, 33, 32, 31, 30, 29 (IGN1–IGN8 respectively) | Candidate only; confirm logic-level outputs, polarity and failsafe state |
| Analogue sensor inputs | J103: 13–18, 24–28 (AV1–AV11) | Verify voltage protection and ADC conditioning |
| Temperature sensor inputs | J103: 19, 30, 20, 31 (AT1–AT4 respectively) | Verify pull-ups and analogue scaling |
| Knock sensors | J103: 34, 35 | Requires suitable analogue knock frontend |
| Sensor 5 V | J103: 9, 21, 32 (rail 1); 10, 22, 33 (rail 2) | Verify source, current budget and grounds |
| Crank/cam | **No connector pins allocated** | Must trace existing unlabeled pins and provide protected trigger conditioning |
| Wideband CAN H/L | **No connector pins allocated** | Must trace existing unlabeled pins and provide CAN transceiver and termination strategy |
| Battery, switched power, sensor ground, power ground | **Not yet resolved** | Mandatory full netlist and power-domain review |
| USB/console | **No connector pins allocated** | Prefer independent verified USB service connector; USART2 pin declarations alone do not imply an external connector route |

### Critical integration constraint

The selected J102/J103 pins may not provide all essential power, ground, crank/cam and CAN connections after excluding J101. **Do not repurpose any position marked unresolved** until tracing its actual net in the source schematic and PCB. If there is insufficient capacity, revise the connector allocation or add a dedicated service connection explicitly; do not claim a complete usable ECU pinout prematurely.

### MCU mapping workflow

1. Resolve every unlabeled connector pin by netlist and document its electrical function.
2. Draw the Tunuino STM32F429ZIT6 PCB netlist, separating external connector pin, conditioning/driver circuit and MCU pin.
3. Assign two independent trigger-capable timer inputs for crank and cam, ADC channels for sensors, CAN1 RX/TX to a physical CAN transceiver, and USB or serial service pins without alternate-function conflicts.
4. Allocate eight independent timed injector controls and eight smart-coil logic outputs, with hardware disable circuitry that defaults OFF during reset and firmware faults.
5. Generate and review a machine-checkable pin map before enabling any output in firmware.
