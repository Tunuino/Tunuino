# Console telemetry reception

`console/telemetry-stream.js` is a read-only incremental parser for the Tunuino protocol v1.

- Handles partial serial chunks, concatenated frames and leading noise.
- Rejects oversized frames, CRC mismatches and invalid telemetry payloads.
- Resynchronizes after invalid data and only publishes validated telemetry.
- Bounds its internal byte buffer by frame length during normal operation.

The UI can use `TelemetryStream` to receive validated RPM, MAP, TPS, CLT, IAT, AFR and battery data.

**No physical USB/serial transport has been implemented.** Data received by this parser is not proof of a connected ECU. Never treat telemetry CRC as authentication or use this module to command ignition or injectors.
