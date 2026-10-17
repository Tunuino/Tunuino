# STM32F429 auxiliary output allocation gates

**Current stage: interface engineering only.** The approved J102/J103 connector map and twelve auxiliary contacts are recorded. **No STM32F429 GPIO-to-driver assignment is electrically verified yet.** The Proteus F4 MCU is not pin-compatible with the Tunuino STM32F429 design.

## Driver groups to implement in KiCad

| Output group | J102 contacts | Required circuit | Allowed draft functions |
|---|---|---|---|
| High-side auxiliary (4) | 1, 2, 13, 14 | Four protected high-side switches with defined OFF at reset | Digital |
| Low-side auxiliary (8) | 8, 9, 10, 11, 12, 20, 21, 23 | Eight protected low-side drivers rated for selected loads and switching frequency | Digital; PWM only after thermal/EMC validation |
| Idle stepper (4 motor wires) | 7, 19, 30, 29 | Two protected full H-bridges or dedicated bipolar stepper driver | Dedicated stepper |
| Injectors (6) | 3, 15, 4, 16, 5, 6 | Six saturated injector drivers | Injector only |
| Smart-coil logic (6) | 35, 34, 22, 33, 32, 31 | Six protected smart-coil logic interfaces | Ignition only |

## Before assigning GPIOs

1. Select driver part numbers and document control input thresholds, output polarity, diagnostics and worst-case currents.
2. Create the STM32F429ZIT6 symbol sheet and list every consumed peripheral: USB, SWD, clock, CAN, serial console, CKP/CMP timer capture, analog inputs, injector/ignition timing, PWM and stepper control.
3. Allocate MCU pads and alternate functions against the STM32F429 datasheet, checking timer channel sharing, DMA/interrupt priorities and reset-state pull resistors. Record conflicts in a machine-readable table.
4. Add actual KiCad circuits, power distribution, transient protection, driver thermal design, grounding and diagnostics; verify with ERC and datasheet reviews.
5. Only after bench validation may an output's `UNVERIFIED` status change. Until then the configuration menu may display planned functions but must not arm hardware.

## Console configuration rules

Expose only driver-supported modes; reserve injector/ignition/stepper power terminals. PWM requires an explicitly approved frequency range and timer assignment. Reject overlapping assignments, changes while outputs are armed, incompatible voltage/current demands and DBW activation. Persist configuration only after full validation. A user-selected output mode cannot reconfigure or replace a physical H-bridge or power driver.

**Manufacturing:** This document is not a schematic, routed PCB, fabrication package or verification report.
