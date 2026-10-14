# RPM freshness checks

Firmware `console_rpm_freshness` accepts a valid running RPM only when the most recent CKP edge is no older than a caller-selected maximum age in microseconds. The unsigned timestamp subtraction tolerates 32-bit timer wraparound. The caller must choose a threshold suitable for cranking and the minimum expected RPM.

A separately verified stop condition can still report RPM=0. A CKP timeout alone does not prove engine stop.

Console `TelemetryFreshness` tracks the age of the most recently received telemetry message and expires it once when its timeout is exceeded. It measures **arrival time**, not the original sensor sample age. A transport-level heartbeat cannot prove the validity of individual sensors.

No STM32 peripheral is enabled. Neither component by itself integrates with physical CKP capture or authenticates a device.
