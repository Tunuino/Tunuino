# RPM acquisition and publication pipeline

`console_rpm_pipeline` joins CKP/CMP events, engine state polling, CKP edge freshness and partial Console telemetry snapshot creation.

The pipeline does not fabricate CKP edges or RPM. It emits a valid running RPM only when engine-state sync and speed validity are established and the last CKP edge is sufficiently recent. A timeout invalidates the reading, notifies the Console via a cleared validity bit, and requires reacquisition of CKP synchronization.

A separate `confirm_stop` call can explicitly authorize valid RPM=0 when engine state is stopped and speed invalid. CKP or CMP activity automatically revokes that confirmation. **Do not call confirm_stop based only on missing CKP pulses**.

No physical input capture, UART, injectors or ignition are enabled. The firmware STM32 main loop is unchanged. Tests include controlled state injection to isolate publication rules; this is not proof of physical CKP synchronization or successful recovery on a real 60-2 wheel.
