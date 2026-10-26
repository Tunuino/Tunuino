# Proteus v0.7 / STM32F427 input and calibration profile 0.2.0 (16×16)

The ChibiOS image implements acquisition, calibration storage and USB access.
It does not initialize actuator GPIO or schedule injection, ignition, throttle,
fuel pump or other engine outputs. VE and ignition tables are stored data only.
Physical ADC accuracy, trigger polarity/timing, flash persistence and operation
with the actual TunerStudio application still require bench validation.

## Inputs and time

The assignments in `hardware/review/proteus-v07-inputs.json` were checked against
the pinned Proteus PCB and the STM32F427 LQFP144 pin table. The user selected the
physical F427 variant; the source schematic's F427/F767 part-number ambiguity
remains recorded in the hardware review.

ADC1 scans 16 channels using DMA2 stream 4, four scans per conversion, averaged
per channel, at a nominal 10 ms interval. The ordering is AV1..AV11,
12V_DIVIDED, AT1..AT4. A conversion older than 10 ms is discarded; data older
than 100 ms is invalid. ADC GPIOs are analog inputs. Displayed voltage is at the
MCU pin: `counts * reference_mV / 4095000`. External divider ratios are not
assumed and this is not the battery voltage.

PE7 (VR1_OUT, crank) and PE8 (VR2_OUT, cam) use rising-edge EXTI events and
external pull-ups R1215/R1216. TIM2 runs at 1 MHz. Timestamps are taken in the
interrupt handler, not by timer input capture. A bounded 64-event ring feeds
the portable missing-tooth decoder. An overflow or clock/capture failure
invalidates RPM until capture is reinitialized. Flash operations can stall
acquisition; validity is checked again afterwards. USB startup/recovery and a
committed configuration change resynchronize capture. The software does not
establish a physical TDC or actuator timing reference.

## Wire profile

Identity is Proteus/F4, revision 7, firmware 0.2.0, capabilities 15: live data,
map reads, partial live data and this IO profile. The signature is
`Tunuino Proteus v0.7 F427 IO v2`. The matching definition is
`firmware/tunerstudio/generated/tunuino_proteus_v07_f427_io.ini`.
The former 0.0.3 identity-only profile remains a separate regression fixture.

For the serial discovery command `F`, this profile advertises `001`,
the CRC-wrapped TunerStudio protocol also used by the reference implementation.
Plain `Q`/`S` discovery replies include a trailing NUL. Physical TunerStudio
3.3.01 diagnostics showed that CRC-framed `S` with an additional NUL is rejected
as unsupported firmware, even when the visible signature matches. The CRC-framed
`S`/`Q` replies therefore contain the exact ASCII signature bytes declared in
the INI (no trailing NUL); `V` keeps its NUL terminator. The realtime
blocking factor is 100 bytes to cover the complete 100-byte runtime block.
Table transfers retain a 64-byte blocking factor. The Java Console continues
to use its independent `TUNUINO?` identity request.

Commands use the existing length/CRC envelope. Multi-byte numbers are big endian.
Response status is a separate first byte: 0 success, 0x83 command/bounds error,
0x84 invalid configuration, 0x80 persistence failure. Command errors preserve
the session; envelope corruption or transport timeout resets/re-enumerates CDC.

| Command payload | Result |
|---|---|
| `S`, `Q`, `V`, `F` | Signature, version or envelope revision |
| `A` | Status followed by 100 runtime bytes (legacy clients) |
| `O` + offset U16 + count U16 | Status plus the requested range of 1..100 runtime bytes |
| `TUNUINO:E` | Read-only VE/ignition table preview v1, with explicit sensor and calibration validity; never actuates the engine |
| `TUNUINO:T` | Read-only **dry-run** revolution/cycle/tooth timing, relative air-load index and signed advance-to-reference interval, only when map preview is valid |
| `TUNUINO:V` | Read-only staged validation v1: invalid setting groups, pending/persisted state, active generation and page-1 CRC |
| `r` + page/offset/count, each U16 | Read staged bytes, count 1..256 |
| `w` + page/offset/count U16 + bytes | Stage bytes without applying them |
| `b` + page U16 | Validate, persist and activate all three staged pages |
| `k` + page U16 | CRC32 of the staged page, U32 |
| `D` | Discard all staged edits; restore active bytes |
| `TUNUINO:P` | Partial live frame v2, independent validity per channel |

The INI revision 5 uses ranged `O%2o%2c` runtime reads while retaining
the old `A` request. Reads outside the 100-byte block receive status 0x83,
without ending the serial session. The read-only native `TUNUINO:E` preview
returns 20 bytes including status: version 1, validity, reason bits, signed
ignition degrees x100, VE percent x100, measured RPM/MAP and the active
configuration generation. No outputs are scheduled or enabled by this command.
Missing RPM, MAP, synchronization, ADC, commissioning or table range forces a
zeroed computed value; the caller must check validity before displaying
numbers. Factory maps are zero placeholders, not an engine calibration.

The independent portable `engine_math` module implements a **non-actuating**
integer-only model: revolution and four-stroke-cycle duration, nominal tooth
slot time, a dimensionless relative air-load index (`VE% * MAPkPa / 100kPa`)
and signed interval between ignition advance and a hypothetical reference
point. No injector pulse width, spark dwell, physical TDC offset, cylinder
phase, fuel-air target, compensation tables or GPIO scheduling are calculated.
The `TUNUINO:T` command returns status plus 31 bytes with version 1, validity,
reason flags, measured RPM, revolution/cycle/slot microseconds, load index
multiplied by 100, signed reference interval in microseconds and configuration
generation. All derived values remain zero when the prerequisite `TUNUINO:E`
calculation is invalid.

The Console also offers `Calcular tempos e carga teórica` in the Maps page.
The Java `EngineTimingClient` rejects inconsistent timing fields, overflow,
invalid periods and status before displaying anything. Older IO firmwares
responding `0x83` to the new command produce a clear unsupported-feature
message **without closing the working COM port**. The button is disabled
while calibration changes or live recording own the USB worker.

The Windows Console now has a Maps-panel read-only preview button. Its Java
client validates the 20-byte `TUNUINO:E` response, decodes signed spark
advance, makes all nonzero validity reasons visible and never interprets a
blocked computation as a usable engine value. It also implements ranged
`O` runtime reads, with strict range and response checks. The operations
run on the existing exclusive serial worker and are disabled while datalog
monitoring or map transactions are in progress. The preview is not an engine
tune, is not logged as a commanded output and does not drive hardware.

The legacy `TUNUINO:L` request still requires all four signals. Missing fields
in partial frames are zero with validity bits clear. Consumers must use the
mask rather than interpreting those zero bytes as measurements. Console CSVs
add `valid_mask` and leave unavailable channel columns blank. Legacy five-column
CSVs remain readable. Graphs break at missing values.

Runtime offsets exclude the status byte:

| Offset | Type | Meaning |
|---|---|---|
| 0..30 | bytes | Identity payload without status |
| 31 | U08 | Reserved zero |
| 32 / 36 | U32 / U16 | Uptime seconds / millisecond fraction |
| 38 | U08 | Validity bits: RPM 1, MAP 2, lambda 4, coolant 8 |
| 39 | U08 | ADC valid 1, staged edits 2, persisted 4, crank valid 8 |
| 40 / 44 / 48 / 52 | U32 / U32 / U32 / S32 | RPM ×1000, kPa ×1000, lambda ×1000000, coolant C ×1000 |
| 56 | U32 | Active configuration generation |
| 60..91 | 16 × U16 | Raw ADC counts in the ordering above |
| 92 / 94 / 96 | U16 / U16 / U32 | ADC reference mV / tooth index / capture errors |

## Native 16×16 settings and maps

The F427 IO v2 application uses **only 16×16** for both VE and ignition.
Its versioned firmware identity is 0.2.0 and its TunerStudio signature is
`Tunuino Proteus v0.7 F427 IO v2`. Configuration schema is 2; the INI has
three pages sized **256, 576, 576** bytes (1,408 bytes total).

Page 1 contains the inputs, trigger, sensor calibration and ADC reference
using the established 256-byte layout, except schema value 2 at offset 0.
Both map pages have 16 U16 RPM points (offsets 0..31), 16 U16 kPa×100
points (offsets 32..63) and 256 signed U16/S16 map cells
(offsets 64..575), row-major. VE values are positive percentage×100;
ignition advance is signed degrees×100. All axes strictly increase.
Defaults deliberately have zero map cells and disabled sensor selection:
the factory configuration is NOT an engine tune.

The Console map editor reads complete 16×16 maps, stages each page in
bounded 64-byte writes, burns the validated 1,408-byte configuration and
verifies the native map by full readback with CRC32. A Console for the
old firmware is rejected by the changed device identity. Files for
earlier map formats can be imported and converted **one way** to
16×16 local files; the ECU itself has no 8×8 page or 8×8 operating mode.
No actuator GPIO is enabled by any map operation.

## Flash schema transition and backup requirement

The F427 firmware reserves STM32F427 flash sectors 5 and 6, starting at
0x08020000 and 0x08040000 (128 KiB each), for an append-only journal.
The 0.2.0 journal records have **1,440 bytes** and schema-2 magic
`TNUIO002`: 16-byte header, 1,408 configuration bytes, CRC32,
8 erased guard bytes and a four-byte commit marker programmed last.
An interrupted write is rejected by the commit marker/CRC.
The other sector is erased on rollover only after a valid record was selected.

**Compatibility warning:** records from firmware 0.1.0 are not read as
schema-2 configurations. On the first boot of v0.2.0, if only old records
exist, factory-disabled inputs and zeroed 16×16 maps are used.
The prior flash sectors are not automatically reinterpreted as new maps,
but a subsequent Burn can eventually overwrite them. Back up the old
calibration externally before upgrading. Do not test new firmware on an
installed engine or connected actuators. Flashing from 0x08000000 replaces
the installed application/bootloader. A full ECU restore requires an
appropriate prior firmware and calibration backup.

Existing USB CDC identification, CRC32 framing, realtime 100-byte
sensor record, native `TUNUINO:E` preview and `TUNUINO:T` mathematical
dry-run are retained. No injector/coil timing outputs are implemented.
TunerStudio compatibility still requires validation with its real Windows
application, independently of passing automated INI and communication tests.

## Verification

`tools/check_core.py` checks the actual portable C profile against Java clients,
calibration, partial sensors/CSV, signed map writes and full readback. The flash
model interrupts each of 361 programming steps and tests corruption and bank
rollover. The actual ChibiOS main loop is compiled with a modeled USB driver,
including recovery followed by a valid request. `tools/check_io.py` independently
compares INI fields, page commands, signed units and CRCs with C wire vectors.
The ARM builder verifies linked ADC, EXTI, DMA and USB vectors, image bounds,
descriptors and the INI signature. These checks do not emulate electrical IO,
the STM32 flash controller or the TunerStudio desktop application.
