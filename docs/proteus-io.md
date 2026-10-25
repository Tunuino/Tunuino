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

Commands use the existing length/CRC envelope. Multi-byte numbers are big endian.
Response status is a separate first byte: 0 success, 0x83 command/bounds error,
0x84 invalid configuration, 0x80 persistence failure. Command errors preserve
the session; envelope corruption or transport timeout resets/re-enumerates CDC.

| Command payload | Result |
|---|---|
| `S`, `Q`, `V`, `F` | Signature, version or envelope revision |
| `A` | Status followed by 100 runtime bytes |
| `r` + page/offset/count, each U16 | Read staged bytes, count 1..256 |
| `w` + page/offset/count U16 + bytes | Stage bytes without applying them |
| `b` + page U16 | Validate, persist and activate all three staged pages |
| `k` + page U16 | CRC32 of the staged page, U32 |
| `D` | Discard all staged edits; restore active bytes |
| `TUNUINO:P` | Partial live frame v2, independent validity per channel |

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

Each map has eight U16 RPM bins at offset 0, eight U16 kPa ×100 bins at 16 and
64 S16 cells at 32, row-major (load row, RPM column). VE uses percent ×100,
ignition degrees ×100, including negative advance. Axes must strictly increase.
Tables start with zero cells and are not engine tunes. Native `.tnmap` units
have three decimal places; Console writes reject values that cannot be
represented exactly with two decimal places. The Console writes a whole map,
commits it and compares a complete native readback before reporting success.
It refuses to overwrite pre-existing staged changes.

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
