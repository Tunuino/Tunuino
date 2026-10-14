# Synthetic 60-2 CKP recovery test

The host test feeds synthetic 60-2 timestamps into the real engine-state and Console RPM pipeline code. It checks acquisition of CKP gap sync, publication of approximately 1000 RPM, stale-reading rejection, timeout-induced sync loss, and reacquisition.

This simulation validates software behavior only, with an idealized stable CKP tooth interval. It does not model sensor conditioning, electrical noise, jitter, missing/extra teeth, real timing jitter, engine acceleration, or physical STM32 input capture. No ignition or injection outputs are activated.
