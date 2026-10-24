# TunerStudio USB bench profile, layout 1

The definition is `firmware/tunerstudio/generated/tunuino_proteus_v07_f427_usb_bench.ini`.
The build copies it next to the BIN, records its SHA-256 and verifies that its
signature is embedded in the linked image. Kit and release packaging reject a
missing, modified or mismatched definition. INI changes select a complete CI kit.

This profile is enabled only for Proteus revision 7 / F4, firmware 0.0.3 and zero
engine capabilities. It exposes identity and an immutable schema byte. It does
not expose sensor values, calibration, output tests or engine commands. The
existing `TUNUINO?` identification remains available to the Windows Console.

| Request | Response data |
| --- | --- |
| `Q`, `S` | `Tunuino Proteus v0.7 F427 USB bench v1`, with trailing NUL |
| `V` | `Tunuino 0.0.3 Proteus v0.7 USB bench`, with trailing NUL |
| `F` | Three ASCII bytes `001` |
| `A` | 31 identity bytes, as below |
| `R` | One immutable schema byte, value 1 |
| Any other profile command | MS envelope error `0x83`; no write or burn |

All INI commands use `msEnvelope_1.0`: a two-byte big-endian body length, body,
and four-byte big-endian IEEE CRC32 of the body. Replies start with status zero;
TunerStudio removes that status before interpreting output-channel offsets.
Discovery also accepts bare `F`, `Q`, `S`, `V` at an empty frame boundary and
returns the data without envelope/status. The maximum framed body is 4096, so
its length's high byte cannot be confused with these ASCII commands. A probe
inside an incomplete frame remains frame data. Corruption and transport
timeouts retain the existing session-failure behavior.

| INI offset | Bytes | Field |
| --- | --- | --- |
| 0 | 7 | ASCII `TUNUINO` |
| 7 | 1 | Native identity protocol version |
| 8 | 1 | Board family: 1 Proteus |
| 9 | 1 | MCU series: 1 F4 |
| 10 | 2 | Hardware revision: 7 |
| 12 | 3 | Firmware major, minor, patch |
| 15 | 4 | Engine capabilities: zero |
| 19 | 12 | STM32 UID |

Multibyte identity fields use big-endian order. The `A` reply reuses the native
identity serializer; there is no second struct layout. The one-byte constants
page has empty write/burn commands and a read-only extension. Direct write
attempts return an error and leave the session usable.

`tools/check_tunerstudio.py` compares real C link responses with the distributed
INI using independent Python CRC and channel decoding, and exercises package
rejection cases. The ChibiOS host model also feeds coalesced bare and framed
requests through the real board main loop, with delayed USB TX completions.
These tests use simulated transports; they do not run TunerStudio or a physical
ECU. User instructions and physical results belong in
[Tunuino_documents](https://github.com/Tunuino/Tunuino_documents).

Protocol references: [EFI Analytics ECU definition specification](https://www.efianalytics.com/TunerStudio/docs/EFI%20Analytics%20ECU%20Definition%20files.pdf)
and [MS serial envelope](https://www.msextra.com/doc/pdf/Megasquirt_Serial_Protocol-2014-10-28.pdf).
