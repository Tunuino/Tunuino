# Tune maps, initial data model

The first tune map model has separate 8x8 VE and ignition maps. Each has strictly increasing RPM and load axes, 16-bit signed cell values and bilinear interpolation with endpoint clamping. Units: load = kPa x10, VE = percent x10, ignition = degrees BTDC x10.

This module is deliberately detached from any fuel pulse or spark output. It does not provide calibrated default maps, writable TunerStudio pages, flash persistence, validation of safe tuning ranges, or a physical ECU scheduler.

Next: add range constraints and an explicit read-only tune snapshot shared by both UI protocols. Only after transport and hardware verification should write commands be considered.
