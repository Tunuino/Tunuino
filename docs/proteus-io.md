# Proteus v0.7 / STM32F427 input and calibration profile 0.1.0

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

Identity is Proteus/F4, revision 7, firmware 0.1.0, capabilities 15: live data,
map reads, partial live data and this IO profile. The signature is
`Tunuino Proteus v0.7 F427 IO v1`. The matching definition is
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

The INI revision 4 uses ranged `O%2o%2c` runtime reads while retaining
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
invalid periods and status before displaying anything. Older 0.1.0 firmwares
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

## Settings and maps

`device.h`, `device.c` and the generated INI define schema 1. Page 1 has 256
bytes; VE page 2 and ignition page 3 each have 160. Page 1 contains trigger
enable/wheel settings, independent channel selectors, MAP/lambda two-point
curves, a three-point coolant curve, maximum RPM and ADC reference voltage.
Raw breakpoints must increase. Reserved bytes must be zero. Sensors and trigger
start disabled. Default physical calibration values are zero placeholders;
selecting an uncalibrated sensor is rejected at Burn.

The Console Inputs workspace now reads/edits page 1, validates the staged data
with the firmware, persists and checks a full readback. It preserves both maps,
rejects pre-existing pending edits and detects stale configuration snapshots.
It also displays the 16 raw ADC channels, their MCU-pin voltages and capture
state, with no fabricated values when a signal is unavailable. File backups
and exact decimal calibration are documented in [console-configuration.md](console-configuration.md).

Each map has eight U16 RPM bins at offset 0, eight U16 kPa ×100 bins at 16 and
64 S16 cells at 32, row-major (load row, RPM column). VE uses percent ×100,
ignition degrees ×100, including negative advance. Axes must strictly increase.
Tables start with zero cells and are not engine tunes. Native `.tnmap` units
have three decimal places; Console writes reject values that cannot be
represented exactly with two decimal places. The Console writes a whole map,
commits it and compares a complete native readback before reporting success.
It refuses to overwrite pre-existing staged changes.

## Safe 16x16 map preparation (offline only)

The portable C map library and the Java Console both support a deterministic
**8x8 → 16x16** map refinement operation. It retains all original RPM and load
axis knots and their 64 original cell values exactly. New knots subdivide the
largest available gap, and new cells are bilinear interpolations rounded to
0.001 units. Refusal to refine an axis without enough integer space leaves
the original map unchanged. Independent C and Java implementations produce
byte-for-byte identical 16x16 `.tnmap` files, including CRC.

This **does not** yet change the ECU's active 8x8 page format, INI page
dimensions or flash journal. A 16x16 local file must **not** be burned to the
current F427 firmware: doing so would require a coordinated configuration
version and safe migration of existing flash records. The Console's ECU write
path rejects any size other than 8x8.

In the Console map editor, **Converter 8×8 para 16×16** opens the newly
refined map in another window with the ECU-upload button omitted. The
source map remains unchanged, and the 16×16 map can be saved explicitly
as a local `.tnmap` for later firmware versions.

## Flash journal and USB recovery

The application remains limited to 128 KiB starting at 0x08000000. F427 sectors
5 (0x08020000) and 6 (0x08040000), 128 KiB each, are reserved for configuration.
They must not be used by a bootloader or another application image. Records
are 608 bytes: magic, sequence, length, all 576 setting bytes, CRC and a commit
marker programmed last. Torn records consume their slot. Rollover erases the
other sector while preserving the newest complete record. Identical saves do
not program flash again. Startup accepts only CRC-valid, schema-valid records.
The driver programs aligned 32-bit words, refreshes flash caches and verifies
readback. No option-byte or mass-erase operation is used by the journal.

TIM2 provides the protocol clock across flash stalls. Burn responses have a
fresh TX deadline; clients allow up to 6 seconds per flash exchange. After a
terminal transport fault the main loop disconnects/stops USB, resets CDC queues
and link state, waits 250 ms and starts a fresh enumeration. Saved settings
survive that recovery. A desktop client must reopen the resulting COM session.

## Verification

`tools/check_core.py` checks the actual portable C profile against Java clients,
calibration, partial sensors/CSV, signed map writes and full readback. The flash
model interrupts each of 153 programming steps and tests corruption and bank
rollover. The actual ChibiOS main loop is compiled with a modeled USB driver,
including recovery followed by a valid request. `tools/check_io.py` independently
compares INI fields, page commands, signed units and CRCs with C wire vectors.
The ARM builder verifies linked ADC, EXTI, DMA and USB vectors, image bounds,
descriptors and the INI signature. These checks do not emulate electrical IO,
the STM32 flash controller or the TunerStudio desktop application.
