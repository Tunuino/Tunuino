# Web Serial transport (experimental)

The Console can request a USB/serial port using browser Web Serial on a secure origin (HTTPS or localhost) in a supported browser. The user must grant port permission.

- Port opens at 115200 baud, 8N1 default.
- No bytes are transmitted. This is a read-only transport.
- The connection remains **unidentified** until a CRC-valid HELLO frame arrives.
- HELLO type 1 payload is three bytes: 0x54 0x4E 0x01 (ASCII TN, protocol version).
- Telemetry frames are ignored before identification.
- Disconnect and read errors clear the identified state.
- This is a prototype: no authentication, freshness timeout, device identity attestation, or reconnect policy.

The STM32 firmware **does not yet send HELLO or telemetry**. A serial port being open is not evidence of an attached Tunuino ECU. Never enable engine outputs from this transport.
