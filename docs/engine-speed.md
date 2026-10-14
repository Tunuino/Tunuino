# Engine speed prototype

For a 60-2 crank wheel, nominal slots are 6 crank degrees apart, giving `RPM = 1,000,000 / normal_tooth_period_us`.

The `engine_speed` module computes an integer RPM from validated normal tooth intervals, skips missing-tooth gaps, rejects too-fast glitches and zeroes RPM on timeout. Timestamps are unsigned 32-bit microseconds; subtraction supports wraparound when intervals remain below 2^31 microseconds.

**Integration requirements:** call `normal_tooth` only after the CKP decoder identifies a valid normal tooth; call `gap` on a verified missing-tooth event; call `poll` regularly. Current module does not itself prove sync or manage hardware timer overflow. Input filtering and timer capture must be validated before use.

No ignition or injector output is authorized.
