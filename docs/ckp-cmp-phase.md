# CKP/CMP phase synchronization prototype

This module tracks crank reference events and a single cam pulse per 720 crank degrees. It detects duplicate cam pulses and missing cam pulses once a phase candidate is acquired.

The SYNCED state here is **provisional**. It does not prove that the cam pulse occurred in the correct crank-angle window, that the reference corresponds to a particular cylinder's TDC, or that the engine is safe for sequential fueling.

Required before real engine scheduling:
- Define the exact CKP/CMP wheel pattern and edge polarities.
- Associate each cam pulse with a verified crank tooth/angle window.
- Establish calibrated 720-degree phase and cylinder firing order.
- Enforce crank/cam signal-loss timeouts and robust resynchronization.
- Validate with realistic jitter, acceleration/deceleration, noise and missing pulses.
- Integrate with hardware capture timers and external input conditioning.

No injector or ignition outputs are driven or authorized.
