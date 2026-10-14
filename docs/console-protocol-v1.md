# Tunuino Console protocol v1 (draft)

**Status: offline implementation only.** C firmware encoder and JavaScript Console decoder exist, but neither is wired to a physical USB, UART or CAN transport. No live telemetry is currently available.

## Binary frame

| Offset | Field | Format |
| --- | --- | --- |
| 0 | Sync | 0x54 0x4E (ASCII TN) |
| 2 | Version | u8 = 1 |
| 3 | Type | u8: 1 HELLO, 2 TELEMETRY |
| 4 | Payload length | u16 little-endian, 0..64 |
| 6 | Payload | variable |
| 6+N | CRC | CRC16-CCITT-FALSE, u16 little-endian |

CRC polynomial 0x1021, initial 0xFFFF, no reflection, XOR-out 0. CRC covers bytes from version through payload.

## Telemetry payload v1 (type 2)

Seven consecutive u16 little-endian fields:
1. RPM (rpm)
2. MAP (0.1 kPa)
3. TPS (0.1%)
4. CLT (0.1 C + 40 C offset)
5. IAT (0.1 C + 40 C offset)
6. AFR (0.01 AFR)
7. Battery (0.01 V)

No write commands are defined. HELLO payload format, transport framing recovery, sequence counters, timeouts, authentication/authorization and compatibility negotiation are future work.

**Security and safety:** a valid CRC only detects accidental corruption. It is not authentication. No ECU control or tuning writes are permitted by this protocol.
