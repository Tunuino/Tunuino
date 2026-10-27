# F427 IO configuration workspace

The Console's Inputs workspace edits schema-2 page 1, reads all 16 ADC channels,
and reports firmware validation. It requires the matching Bundle firmware with
`TUNUINO:V`; an older image that lacks this command receives an update
message before staging any edits. Acquisition configuration does not enable
engine actuators. Use measured sensor curves; no physical calibration is inferred.

Reads bracket four 64-byte page reads with validation snapshots and compare
generation, state and an independently calculated page CRC. Writes require the
same identified profile/UID in the UI and an unchanged clean ECU snapshot.
The client stages page 1, verifies all bytes, asks the firmware to validate all
three staged pages, burns once, then requires exact readback, a clean persisted
state and the next configuration generation. VE/ignition pages are preserved.
Identical settings do not issue a Burn. Pre-existing pending edits are rejected.

Before Burn, a failed transaction discards only its own staged edits. Once Burn
has been attempted, failures retain the staged state and instruct the user to
read again before retrying. Lost acknowledgements must never be reported as a
definite rollback. Explicit Discard resets all pending pages, including maps,
and therefore requires a confirmation. Monitoring/map/programmer operations
and configuration transactions share the exclusive serial worker.

`TUNUINO:V` returns 16 status-prefixed bytes, big endian:

| Offset | Type | Meaning |
|---|---|---|
| 0 / 1 | U08 / U08 | Success status 0 / format 1 |
| 2 / 3 | U08 / U08 | Pending edits / previously persisted, each 0 or 1 |
| 4 | U32 | Invalid groups: schema-invalid group 1, trigger 2, MAP 4, lambda 8, coolant 16, ADC 32, VE 64, ignition 128 |
| 8 | U32 | Nonzero active configuration generation |
| 12 | U32 | CRC32 of staged page 1 (256 bytes) |

The query never saves or activates data. Burn uses the same grouped validation;
unused/disabled calibration endpoints must still fit their documented ranges.
The runtime block and serial protocol are unchanged, but the TunerStudio signature now identifies the 16×16 firmware as IO v2. The new INI must be used.

Local `.tncfg` backups contain ASCII `TNCFG002`, U16 length 256, 256 page bytes
and CRC32 of the preceding 266 bytes (270 total). They contain no maps and are
not written to the ECU merely by opening a file. Import validates length,
schema, reserved fields, curve ranges and CRC. Saves use CREATE_NEW. Decimal
edits never round silently. Disconnect retains local edits for backup and
blocks ECU writes; another ECU requires a fresh configuration read. Closing
or replacing an edited form prompts for preserving/discarding the local copy.

ADC voltage is the MCU-pin value `counts * reference_mV / 4095000`; it is not a
battery/divider voltage. An unavailable ADC snapshot displays absent values.
Host tests exercise the C firmware through Java clients, pending/stale edits,
exact units, map preservation, file CRC and persistence failure. JavaFX checks
exercise the real form, invalid/busy/offline gating, ADC scaling and local edits.
These tests do not validate physical ADC accuracy, trigger timing or flash hardware.

## Motor extension

Page 1 now stores activation policy at byte 102 and the engine profile at 128–255.
Existing schema-2 configurations with zero extension bytes remain valid. Backups
include these fields. Validation adds flags 256 (engine profile) and 512
(autonomous prerequisites). See `engine-outputs.md` for layout and startup rules.
Base maps are deliberately labeled local starting references. They are never
reported as ECU readback, nor uploaded simply by selecting a template. The engine
form uses milliseconds and exact decimal conversion to internal timer units.
