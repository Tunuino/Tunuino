# CKP/CMP trigger capture plan

The initial `trigger_capture` module accepts timestamped edges, filters pulses shorter than a configurable minimum, tracks valid periods and detects signal loss. It is platform-independent and unit-testable.

**Important:** VALID means only that a few pulse intervals fell within configured bounds. It does **not** mean that the crankshaft angle, RPM, cylinder phase or cam/crank synchronization is established.

Before engine use, implement:
- Input conditioning and polarity for VR/Hall sensors, including protection and hysteresis.
- STM32 timer input capture, overflow handling and timestamp extension.
- Tooth-pattern decoding (e.g. 60-2) and missing-tooth detection.
- Camshaft phase correlation, crank angle and cylinder synchronization.
- Stall detection, tooth error counters, noise filtering and loss-of-sync fail-safe.
- Hardware-in-the-loop simulation with real pulse trains and electrical testing.

No injector or ignition scheduling is permitted from this module alone.
