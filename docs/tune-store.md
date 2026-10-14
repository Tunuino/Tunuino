# Validated RAM-only tune store

The common tune store owns independent 8x8 VE and ignition maps, validates them at initialization, exposes copy-out snapshots and interpolated read-only lookups, and starts at revision 1.

This is **not a persistence layer** and has no serial write commands. The initial map must be supplied by trusted firmware code; no default calibration is provided. An invalid initialization invalidates the store and prevents further lookups. A snapshot is a plain copy and must be called in a serialized context; it is not safe to race with initialization from another interrupt or task.

Future steps: explicit immutable page encoding, CRC and revision metadata, transport-independent reads, then transactional storage after hardware validation. TunerStudio INI and serial protocol compatibility remain experimental.
