# Configurable output modes (draft)

The Tunuino Console shall offer Disabled, Digital, PWM, and Stepper modes **only when the board's electrical capabilities permit**. A stepper mode represents a complete dedicated motor-driver channel, not a single MCU GPIO or an injector output. Hardware pin capabilities are not yet proven: no production output pin mapping or motor driver is enabled by this change.

Validation rejects reserved outputs, unsupported modes, duplicate assignments and invalid PWM frequency zero. Future implementation must validate timer sharing, PWM range, complementary drive, electrical current and faults; persist settings atomically with a versioned format; and require engine stopped and outputs disabled before applying changes. Injector/ignition channels are safety-critical reserved outputs, never general-purpose configurable pins. DBW remains disabled pending independent safety review.

The initial module is **configuration validation only**; it does not change GPIO muxes, start timers or energize actuators.
