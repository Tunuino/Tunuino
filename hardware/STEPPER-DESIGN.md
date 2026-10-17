# Tunuino idle stepper: electrical implementation contract

**Status:** design requirements, not a finished circuit or a released PCB. Target is a **four-wire bipolar idle air stepper** using J102 positions 7, 19, 30 and 29. The actuator part number, winding resistance and rated current are not yet known, so a specific driver IC cannot be responsibly finalized.

## Wiring and topology

| Motor terminal | Connector | Pin | ECU internal circuit |
|---|---|---:|---|
| Winding A end 1 | J102 | 7 | H-bridge A output 1 |
| Winding A end 2 | J102 | 19 | H-bridge A output 2 |
| Winding B end 1 | J102 | 30 | H-bridge B output 1 |
| Winding B end 2 | J102 | 29 | H-bridge B output 2 |

These four contacts are **power outputs**, not STM32 GPIO pins. Two independent, protected full H-bridges or a purpose-built bipolar stepper driver are required. No motor terminal is to be tied directly to chassis ground or +12 V. Validate winding pairs with an ohmmeter before connection.

## Required schematic components

1. Motor supply derived from a protected, fused automotive rail; calculate worst-case winding current, contact ratings and driver thermal dissipation before selecting a part.
2. Bipolar stepper driver with two bridges, adjustable or otherwise validated current limiting, undervoltage/overtemperature/short-circuit protection and defined outputs-disabled power-on state.
3. MCU control lines through suitable voltage-domain interfaces, with external hardware pulls that **disable both bridges** during reset, programming and brownout.
4. Independent hardware inhibit path controlled by watchdog/power supervisor. Firmware alone must not be the sole protection against unintended actuation.
5. Automotive-rated transient and EMC filtering appropriate to the selected driver, PCB return routing and cable length.
6. Diagnostic/status connection back to the MCU where supported, plus accessible measurement points for driver supply and fault status.

## Firmware and bench-test acceptance criteria

- Start with driver ENABLE held inactive and outputs physically high impedance; do not step on startup or after fault.
- A stall, brownout, watchdog timeout, loss of MCU clock or driver fault disables outputs without waiting for a software loop.
- Validate winding identification, step direction, peak/RMS current, heating, cable transients and recovery from faults on a current-limited bench supply.
- Determine a safe homing strategy from the actual idle valve datasheet; never assume an arbitrary number of end-stop steps is safe.
- DBW is **not** connected to these bridges. It requires its own separately safety-reviewed motor stage and sensor architecture.

**Manufacturing gate:** no stepper circuit footprint, netlist or PCB traces may be considered finalized until the actuator electrical specifications and driver datasheet have been reviewed.
