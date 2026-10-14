# Validated Console telemetry snapshots

The snapshot builder bridges `engine_state` RPM and the six remaining sensor values to the existing Console protocol v1 payload.

The current protocol has **no per-field validity bits**. To avoid displaying invented or stale zero readings, a snapshot is rejected unless engine-running state and RPM validity are set, RPM fits in u16, all six sensor validity flags are set, and basic value bounds are met.

Validity flags must come from sensor acquisition with proper calibration, fault detection and freshness checks. Merely setting the mask does not validate physical sensors. The builder does not read ADC, CAN, or CKP peripherals and is not connected to STM32 `main.c`.

Current limitation: the Console cannot display RPM alone while other sensors are unavailable; stopped-engine RPM=0 is also not transmitted. A future protocol version should add explicit per-field validity and timestamps to support partial telemetry and engine-off states safely.
