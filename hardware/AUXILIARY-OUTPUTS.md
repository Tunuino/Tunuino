# Auxiliary output inventory (provisional)

This inventory maps logical software output IDs to the **draft J102 connector net names**, not to STM32F429 GPIOs. Every entry has status `UNVERIFIED` and **must remain disabled in firmware**. The DIGITAL/PWM designations are candidate circuit requirements, not claims that a working driver or timer exists. The software must not load these entries as an enabled hardware capability table.

See [auxiliary-output-inventory.csv](auxiliary-output-inventory.csv). High-side outputs require appropriate protected high-side switches; low-side outputs require protected low-side switches. PWM requires suitable switch frequency/thermal behavior and a proven MCU timer route. Outputs must have defined off-state during reset and faults.

Reserved and non-configurable: INJ1–6, IGN1–6, STEPPER_A1/A2/B1/B2, power and sensor pins. DBW remains disabled. Once the complete schematic is drawn, replace each provisional entry with validated driver type, current rating, MCU pin, timer channel, PWM frequency range, electrical protection and fault diagnostics. Add automated checks before permitting a configuration to reach hardware.
