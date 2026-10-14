# TunerStudio integration (experimental)

This is the first read-only prototype, **not a confirmed working TunerStudio connection**.

- Firmware protocol: `firmware/core/tunerstudio_protocol.[ch]`.
- Candidate definition: `tunerstudio/tunuino-prototype.ini`.
- ASCII `S` returns `Tunuino-TS-0.1` without terminator.
- ASCII `A` returns eight bytes, little-endian: RPM u16, MAP (kPa * 10) u16, battery (V * 100) u16, reserved u16.
- Invalid measurements are encoded as 65535. The prototype INI currently displays that raw sentinel; production gauges must hide it.
- All write commands are rejected; no maps, flash writes, ignition or injection are enabled.

Next steps: verify TunerStudio INI syntax and actual serial command framing against documentation and the application; implement a bounded serial parser and read-only transport; then validate the real desktop connection. Do not connect this prototype to physical outputs.
