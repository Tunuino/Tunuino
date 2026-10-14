# Integrated engine state prototype

The `engine_state` module combines the 60-2 missing-tooth decoder, provisional CMP phase, relative crank angle and a simple RPM estimator.

A crank signal timeout invalidates RPM, running status and all position-validity flags. Decoder errors also invalidate RPM.

**Limitations:** The CKP input is not electrically conditioned here, timestamping is not yet connected to an STM32 timer, the missing-tooth decoder and phase validation are provisional, and RPM is calculated from individual intervals without acceleration filtering. The engine-running flag is a software observation, **not** an authorization to energize coils or injectors. This code does not yet implement output scheduling or an engine-safe hardware interlock.

Next: robust tooth decoding with realistic waveform simulations; verified STM32 timer input capture and overflow handling; actual CKP/CMP electrical interfaces and pin mapping.
