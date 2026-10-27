# TPS, idle and boost contract

Page 4 adds 256 bytes at settings offset 1408. Total settings size is 1664;
page 1 (256), VE (576) and ignition (576) retain their byte layouts. All-zero
page 4 means disabled. Format 1 retains calibration while modes are disabled.
The native `TUNUINO:V` validation mask adds 2048 for page-4 settings, sensor
requirements and LS ownership. Writes/Burn remain refused while outputs are armed.

## Wire layout (big endian)

| Byte | Type | Field |
|---|---|---|
| 0 | U8 | Format 0 factory disabled, 1 configured |
| 1 / 2 | U8 / U8 | Idle mode 0 off, 1 temperature duty, 2 RPM PI / LS 1–16 |
| 3 / 4 | U8 / U8 | Boost mode 0 off, 1 fixed duty, 2 MAP PI / LS 1–16 |
| 5 | U8 | TPS AV input 0 off, 1–11 |
| 6 / 8 | U16 | TPS closed/open ADC, 0–4095, unequal; either direction |
| 10 / 12 | U16 | Idle frequency 10–300 Hz / boost 10–100 Hz |
| 14 / 16 / 18 | U16 | Idle min/max/cranking duty in centi-percent |
| 20 / 22 | U16 | Idle maximum TPS centi-% / RPM |
| 24 / 26 | U16 | Idle P/I, centi-% per 100 rpm / per second |
| 28 / 30 / 32 | S16 | Coolant curve temperatures, centi°C |
| 34 / 36 / 38 | U16 | Target RPM at those temperatures |
| 40 / 42 / 44 | U16 | Base duty at those temperatures, centi-% |
| 46 | U16 | Cranking RPM threshold |
| 48 / 50 / 52 | U16 | Boost min/max/base duty, centi-% |
| 54 / 56 | U16 | Target/cut MAP, centi-kPa absolute |
| 58 / 60 | U16 | Boost minimum RPM / TPS centi-% |
| 62 / 64 | U16 | Boost P/I, centi-% per 10 kPa / per second |
| 66 | U16 | TPS endpoint tolerance, centi-percent of ADC span, 0–10% |
| 68–255 | bytes | Zero reserves |

Modes enabled require a valid motor profile, calibrated TPS and distinct LS
channels. Idle additionally requires the coolant input; boost requires MAP
calibration covering the cut pressure. TPS cannot share a selected MAP/lambda
input. LS ownership includes every injector, pump, fan, main relay and the other
PWM valve. An active page prevents the legacy volatile profile opcode from
reassigning these pins. Configure the persistent motor profile instead.

## Control and fail states

TPS is valid only with a fresh ADC snapshot and within the measured endpoints plus the configured tolerance (reference 5% of ADC span); values in that tolerance clamp to 0/100%. Beyond it,
invalid TPS commands both valves OFF. Idle requires closed throttle and RPM
below the configured maximum. Before running synchronization/below cranking RPM
it uses the cranking duty; when running, it interpolates target/base over three
coolant points. Outside temperature knots, it uses the nearest endpoint.

PI terms use a 20 ms update interval and conditional integration with bounded
integral state. Gating/stopping resets the integrator. Idle P is duty percentage
per 100 rpm error; boost P is duty percentage per 10 kPa error. I adds the same
correction per second. No derivative control, stepper idle, electronic throttle,
boost feed-forward table or gear targets are implemented in this revision.

Boost requires running synchronization, valid TPS/MAP, minimum RPM/TPS. Below
these gates it commands duty zero. Reaching the absolute MAP cut while armed
latches engine output fault 16, stops injectors/ignition/auxiliaries and requires
explicit activation or restart. Valve plumbing must make OFF correspond to
wastegate spring pressure; inverted fail-safe plumbing is unsupported.

PWM shares the GPT5 edge/lease ISR with the existing pulse scheduler; TIM2 stays
the microsecond reference. Frequencies round to integer timer periods. PWM edges
are bounded to at least 0.010 ms apart. Duty changes latch on period boundaries;
zero duty immediately clears the pin. Pulses, levels and PWM cannot claim the
same channel. STOP, watchdog expiry or any late edge clears all PWM state and
pins. Register-model tests do not certify physical jitter, valve current,
frequency suitability or load protection.

## Read-only status

`TUNUINO:X` opcode 7 returns 32 bytes: status/version, idle/boost mode at 2/3,
TPS valid/configured at 4/5; U16 TPS/idle duty/boost duty/idle target/boost target
at 6/8/10/12/14; U32 generation at 16; U16 cut MAP at 20; boost/idle gate booleans
at 22/23; U32 reserved LS mask at 24; zeros 28–31. Duties are requested commands,
not electrical/current feedback. TunerStudio derives TPS from the same ADC
values and calibration; detailed PWM command status is in the Console.

## Flash migration and backups

New records use `TNUIO003`, 1664 payload bytes and a 1696-byte record. Boot scans
the new grid and legacy `TNUIO002`/1408/1440 grid in both sectors, selecting the
newest valid sequence with CRC and commit marker. Legacy records gain a disabled
page 4 in RAM. The first new-format save erases/writes the opposite bank, keeping
the legacy bank until commit/readback. Torn new records never replace the last
valid configuration. Subsequent records append normally. Old firmware cannot
read the new format and may recover an older legacy bank; export before downgrade.

`.tncfg` retains page 1, `.tnmap` retains each map, `.tnaux` retains page 4.
The auxiliary file is 270 bytes: `TNAUX001`, U16 size 256, page bytes and CRC32
of the preceding 266 bytes. Create-new saves refuse overwrite; reads enforce
length, CRC, ranges and reserves. It is not a complete ECU backup by itself.
