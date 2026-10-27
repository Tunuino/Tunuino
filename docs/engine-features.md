# Optional fuel strategies and limits

Page 4 retains its 256-byte size. Bytes 0–67 keep the TPS/idle/boost contract.
The extension marker at 68 is 1; 69 is zero. A zero-filled extension preserves
previous behavior. Flash remains TNUIO003, 1664 payload / 1696 record bytes.
The INI revision is 9; the signature and 100-byte runtime block stay unchanged.

U16/S16 fields use big endian. All stored pulse widths are integer timer ticks;
Console and INI display them in **milliseconds**, with three decimal places.
Durations and delays below are integer milliseconds. Percentages use centi-%;
lambda uses 0.001; temperatures centi-C; voltage centi-V; MAP centi-kPa absolute.

| Offset | Type | Field |
|---|---|---|
| 68 | U8 | Extension format 1 |
| 69 | U8 | Zero |
| 70 | U16 | Feature flags |
| 72–75 | 4 U8 | IAT TEMP1–4, battery AV1–11, launch AV1–11, ethanol AV1–11; 0 disabled |
| 76–80 | 3 U16 | IAT raw ADC axis, ascending |
| 82–86 | 3 S16 | IAT temperatures, either direction |
| 88–90 | 2 U16 | Battery raw ADC endpoints |
| 92–94 | 2 U16 | Battery voltage at endpoints, including the external divider |
| 96–100 | 3 U16 | Voltage curve axis |
| 102–106 | 3 U16 | Injector dead time at voltage points, displayed ms |
| 108–112 | 3 U16 | Coil dwell at voltage points, displayed ms |
| 114–118 | 3 U16 | Target-lambda MAP axis |
| 120–124 | 3 U16 | Target lambda |
| 126–130 | 3 U16 | Lambda P, I, symmetric correction limit |
| 132 | S16 | Minimum CLT for lambda feedback |
| 134–140 | 4 U16 | Lambda minimum/maximum RPM, maximum TPS, start delay ms |
| 142–146 | 3 U16 | TPS rate threshold in centi-%/s, initial extra injection displayed ms, decay ms |
| 148–152 | 3 U16 | DFCO cut RPM, resume RPM, maximum TPS |
| 154 | S16 | Minimum DFCO CLT |
| 156–166 | 6 U16 | DFCO delay ms, RPM cut/resume, launch cut/resume, launch ADC threshold |
| 168–172 | 3 U16 | Ethanol ADC at 0/100%, fuel multiplier at 100% ethanol |
| 174–176 | 2 S16 | Ignition addition at 100% ethanol, IAT reference temperature |
| 178 | U16 | Lambda inhibition after transients, ms |
| 180 | S16 | Hot-IAT retard start |
| 182 | U16 | Ignition retard centidegrees per 10 C |
| 184 | S16 | Latched CLT cut threshold |
| 186–188 | 2 U16 | Latched minimum/maximum battery voltage |
| 190 | S16 | Maximum ignition advance while launch input is active |
| 192–214 | 12 S16 | Cylinder fuel corrections ±25%, before dead time |
| 216–238 | 12 S16 | Cylinder ignition corrections ±10 degrees |
| 240–255 | bytes | Zero |

Flags, bits 0–10: IAT density/hot-air retard, voltage curves, lambda target,
lambda PI, TPS acceleration, DFCO, fuel RPM limit, launch, analog ethanol,
latched CLT/voltage protection, cylinder trims. Other bits are rejected.

Enabled functions require a valid persistent engine profile, MAP and CLT.
IAT uses a TEMP input distinct from CLT. Battery/launch/ethanol cannot overlap
each other or selected MAP/lambda/TPS AV inputs. Lambda PI requires a calibrated
wideband analog input, target-lambda enabled and TPS. TPS acceleration/DFCO
require TPS. Launch requires the main RPM limit and a conditioned AV input.
The main limit must be below the existing acquisition/calibration maximum RPM.
Reference numbers and full scalar ranges are in `feature_fields.json`, checked
against the Java enum and generated INI; they are not physical calibrations.

## Calculation and cuts

1. Existing ReqFuel × VE × MAP and cranking/afterstart/warm-up corrections.
2. Optional ideal-gas IAT density ratio `(273.15 + reference)/(273.15 + IAT)`.
   Optional target scaling `1 / target lambda`, interpolated by MAP. VE/ReqFuel
   must therefore refer to lambda 1 before enabling target correction.
3. Linear ethanol fuel/ignition blending from the calibrated analog controller.
4. Lambda PI percentage, then linearly decaying extra TPS acceleration fuel.
5. Per-cylinder fuel correction, then injector dead time exactly once.
6. Pulse-length and cycle-duty validation on every cylinder; no silent clipping.

Three-point voltage curves clamp at their endpoints. Target lambda also clamps
at MAP endpoints; this does not extend the supported VE/ignition map domain.
IAT ignition retard is proportional above its threshold, capped at 15 degrees.
It combines with ethanol and cylinder corrections; final advance limits remain.

Lambda P units are fuel % / lambda, I units % / lambda / second. Measurement
above target adds fuel. PI runs at most every 20 ms, dt capped at 100 ms, with
bounded integral and conditional anti-windup. Invalid/unready lambda returns
to base fueling and resets the integral. No VE learning or persistent fuel trim
is performed. Gates include start delay, CLT, RPM, TPS and inhibition during
acceleration, all cuts, launch, and the configured pause after these events.
Start/inhibition/decay timers accumulate and saturate, including timer wrap;
a delay never restarts just because the 32-bit counter wraps.

TPS acceleration is extra effective injection time with a linear decay. It
triggers when positive TPS change / elapsed sample time reaches the threshold.
The first sample and gaps over 100 ms cannot trigger an acceleration event.
DFCO requires closed TPS, warm CLT, cut RPM and continuous delay; it retains the
cut down to resume RPM and releases when the throttle/temperature gate opens.
RPM and launch use independent hysteresis. Launch also caps advance; this is a
fuel-cut strategy, with no ignition cut, flat-shift, speed or clutch state model.

Fuel cuts cancel pending/active injector pulses immediately once detected by
the engine service. Ignition charging is not truncated: coils complete their
scheduled dwell. Normal RPM/DFCO/launch cuts recover when their gates clear.
Sensor failures required by enabled IAT/voltage/ethanol/TPS/launch functions
latch the existing calibration fault. Enabled protection thresholds and invalid
CLT/voltage latch output fault **32**. STOP/explicit activation rules remain.
All cuts retain the existing GPIO timer watchdog and pulse bounds.

## Native status

`TUNUINO:X` opcode 8 is read-only and returns 40 bytes:
status=0, format=1 at 1, configured at 2, input validity at 3 (TPS/IAT/voltage/
ethanol/launch/lambda bits 0..5). U16 feature flags 4, TPS 6, S16 IAT 8, voltage
10, ethanol 12, target lambda 14, U32 fuel multiplier centi-% 16, S16 lambda
trim centi-% 20, U16 acceleration extra 22 / latency 24 / dwell 26 in timer ticks,
state bits at 28 (DFCO/RPM cut/launch cut/lambda PI/acceleration/latched protection),
zero 29, S16 ignition addition 30, U32 generation 32, zeros 36..39.
Commands and calculations are not measurements of electrical current or actual
injector flow. The existing engine status injection duration remains the common
command before individual cylinder trim. Console presents the latest queried
snapshot; detailed command state is not added to the existing runtime CSV/INI.

## Editing and compatibility

`Combustível e proteções` edits this extension and preserves TPS/valve bytes;
`Ralenti e turbo` edits its own fields and preserves this extension. Either
page can become stale after the other saves, and must read again. Read/stage/
validate/burn/readback and CRC checks remain exclusive, require disarmed outputs,
and refuse stale snapshots/pending changes. A save or reference never arms.
The same `.tnaux` format contains the full page; old 68-byte configurations are
readable by the new Console/firmware. New functions begin disabled on upgrade.
Older firmware/Console reject this extension, even with flags disabled; a
firmware downgrade may select an older record or factory configuration. Export
`.tncfg`, both `.tnmap` files and `.tnaux` before changing firmware versions.

Unit tests cover independent physical-unit vectors, real sequential scheduling,
injector cancellation without coil dwell truncation, protection latching,
sensor collisions, C/Java save/readback and extension preservation. Journal
migration interruption tests now include the extension and cylinder data.
No physical ECU/motor validation is implied.

VVT, knock acquisition, direct frequency ethanol, electronic throttle, stepper
idle, non-missing-tooth decoders, batch/wasted-spark, barometric correction,
AFR 16x16, VE learning/Autotune, long-term trim, CAN engine integration and Lua
are not implemented by this module.
