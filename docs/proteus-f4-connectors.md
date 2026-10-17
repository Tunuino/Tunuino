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

## Tunuino two-connector target allocation (design decision, PCB redesign required)

This section is the **chosen Tunuino external interface**, not the as-built Proteus v0.7 netlist. The source-derived table above remains the reference for the original hardware. Pins marked **REWORK** require electrical schematic and PCB changes; no firmware GPIO should be enabled on the strength of this table alone. Connector numbering must be checked from the mating face before making a harness.

### J102: power outputs (35 positions)

| Pin | Tunuino function | Source net / disposition |
|---:|---|---|
| 1 | High-side auxiliary 2 | HS2 retained |
| 2 | High-side auxiliary 1 | HS1 retained |
| 3 | Injector 1 | LS1, validate saturated driver |
| 4 | Injector 3 | LS3, validate saturated driver |
| 5 | Injector 5 | LS5, validate saturated driver |
| 6 | Injector 6 | LS6, validate saturated driver |
| 7 | Injector 7 | LS7, validate saturated driver |
| 8 | Low-side auxiliary 9 | LS9 retained |
| 9 | Low-side auxiliary 11 | LS11 retained |
| 10 | Low-side auxiliary 13 | LS13 retained |
| 11 | Low-side auxiliary 14 | LS14 retained |
| 12 | Low-side auxiliary 15 | LS15 retained |
| 13 | High-side auxiliary 3 | HS3 retained |
| 14 | High-side auxiliary 4 | HS4 retained |
| 15 | Injector 2 | LS2, validate saturated driver |
| 16 | Injector 4 | LS4, validate saturated driver |
| 17 | Power ground A | REWORK: trace original net, high-current return design |
| 18 | Power ground B | REWORK: trace original net, high-current return design |
| 19 | Injector 8 | LS8, validate saturated driver |
| 20 | Low-side auxiliary 10 | LS10 retained |
| 21 | Low-side auxiliary 12 | LS12 retained |
| 22 | Ignition 3 | IGN3, smart-coil logic output only |
| 23 | Low-side auxiliary 16 | LS16 retained |
| 24 | Switched +12 V sense / supply | REWORK: fused and protected supply architecture |
| 25 | Ignition auxiliary 12 | IGN12 retained, disabled by default |
| 26 | Ignition auxiliary 11 | IGN11 retained, disabled by default |
| 27 | Ignition auxiliary 10 | IGN10 retained, disabled by default |
| 28 | Ignition auxiliary 9 | IGN9 retained, disabled by default |
| 29 | Ignition 8 | IGN8, smart-coil logic output only |
| 30 | Ignition 7 | IGN7, smart-coil logic output only |
| 31 | Ignition 6 | IGN6, smart-coil logic output only |
| 32 | Ignition 5 | IGN5, smart-coil logic output only |
| 33 | Ignition 4 | IGN4, smart-coil logic output only |
| 34 | Ignition 2 | IGN2, smart-coil logic output only |
| 35 | Ignition 1 | IGN1, smart-coil logic output only |

### J103: sensors, trigger, CAN and sensor supply (35 positions)

| Pin | Tunuino function | Source net / disposition |
|---:|---|---|
| 1 | Crank VR+ / conditioned trigger input path | REWORK: dedicated VR/Hall conditioner; do not connect raw VR to MCU |
| 2 | Crank VR- / sensor return | REWORK: dedicated differential conditioner and grounding |
| 3 | Cam VR+ / conditioned trigger input path | REWORK: dedicated VR/Hall conditioner; do not connect raw VR to MCU |
| 4 | Cam VR- / sensor return | REWORK: dedicated differential conditioner and grounding |
| 5 | CAN1 H | REWORK: protected CAN transceiver and bus wiring |
| 6 | CAN1 L | REWORK: protected CAN transceiver and bus wiring |
| 7 | Sensor ground A | REWORK: analogue ground strategy |
| 8 | Sensor ground B | REWORK: analogue ground strategy |
| 9 | Sensor 5 V rail 1 | 5V_SENSOR_1 retained |
| 10 | Sensor 5 V rail 2 | 5V_SENSOR_2 retained |
| 11 | Battery voltage sense | REWORK: divider and protection; not direct battery voltage to MCU |
| 12 | Ignition-switched sense | REWORK: divider, protection and power sequencing |
| 13 | Analogue voltage 1 | AV1 retained |
| 14 | Analogue voltage 3 | AV3 retained |
| 15 | Analogue voltage 5 | AV5 retained |
| 16 | Analogue voltage 7 | AV7 retained |
| 17 | Analogue voltage 9 | AV9 retained |
| 18 | Analogue voltage 11 | AV11 retained |
| 19 | Temperature 1 | AT1 retained |
| 20 | Temperature 3 | AT3 retained |
| 21 | Sensor 5 V rail 1 | 5V_SENSOR_1 retained |
| 22 | Sensor 5 V rail 2 | 5V_SENSOR_2 retained |
| 23 | Sensor ground C | REWORK: verify net and analogue return |
| 24 | Analogue voltage 2 | AV2 retained |
| 25 | Analogue voltage 4 | AV4 retained |
| 26 | Analogue voltage 6 | AV6 retained |
| 27 | Analogue voltage 8 | AV8 retained |
| 28 | Analogue voltage 10 | AV10 retained |
| 29 | Sensor ground D | REWORK: verify net and analogue return |
| 30 | Temperature 2 | AT2 retained |
| 31 | Temperature 4 | AT4 retained |
| 32 | Sensor 5 V rail 1 | 5V_SENSOR_1 retained |
| 33 | Sensor 5 V rail 2 | 5V_SENSOR_2 retained |
| 34 | Knock sensor 1 | KNOCK_1 retained; analogue frontend required |
| 35 | Knock sensor 2 | KNOCK_2 retained; analogue frontend required |

### Non-negotiable implementation rules

- This is a **new PCB design allocation**, not a pinout that can be applied to an unmodified Proteus F4 board or wiring loom.
- The J102 pin 24 switched +12 V allocation is **not** a sufficient ECU power design on its own. Validate current, connector contact ratings, fuse, reverse polarity, transient suppression, ground returns and required permanent/switched feeds. Redesign if the power budget requires more contacts.
- J103 pins 1–4 are **sensor-side** crank/cam wiring. The MCU must see the protected, conditioned logic signals, never raw VR voltage. Hall support needs a separately verified input mode.
- J103 pins 5–6 carry differential CAN H/L on the external connector, **not** MCU CAN RX/TX. Fit a transceiver, common-mode protection and deliberate termination.
- The 8 injector and 8 ignition channel allocations specify external pins only; final MCU GPIO/timer assignment depends on the validated STM32F429ZIT6 board netlist and driver topology.
- Dedicated USB programming/debug (SWD/BOOT) must be accessible via a **separate on-board service header or USB connector**, not an invented signal on J102/J103. This does not add a third automotive harness connector.
- No engine test or output arming before netlist/ERC/DRC review, electrical protection validation and bench measurements.
