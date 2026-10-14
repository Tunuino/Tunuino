# Tune data validation

This initial validation checks both 8x8 VE and ignition tables, strictly increasing axes, RPM from 100 to 16000, load from 10 to 400 kPa, VE from 0 to 300%, and ignition from -40 to +80 degrees BTDC. Values are stored at x10 resolution.

**These are permissive data-integrity bounds, not safe operating limits for any engine.** A map passing validation must not be assumed suitable for a vehicle. There is no persistence, tuning write API, or actuation path. Actual calibration needs engine-specific safety limits, sensor fault handling and hardware testing.
