# Engine position prototype

The `engine_position` module combines the provisional 60-2 decoder and the single-pulse-per-cycle CMP state machine.

- A 60-2 wheel has a nominal 6 crank degrees per tooth slot.
- `crank_angle_deg` is a **relative** tooth angle from the detected missing-tooth gap, not calibrated TDC.
- `cycle_angle_deg` is a **provisional** 720-degree angle. Its validity flag means only that the current simple phase state machine accepted a CMP event.
- This implementation does not interpolate angles between teeth, verify CMP angle windows, or compensate for acceleration.
- Hardware timers, sensor conditioning, loss-of-signal timeout, correct cam phase, TDC offset, firing order and fail-safe logic remain mandatory before engine use.
- The output enable path is not connected. No ignition or injection is authorized.
