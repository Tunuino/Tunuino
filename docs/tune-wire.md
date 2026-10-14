# Read-only tune snapshot wire format, v1

The validated tune store can be serialized into a deterministic 336-byte snapshot. The layout is:

- Offset 0: ASCII `TUNE` (4 bytes)
- Offset 4: format version, u16 little-endian = 1
- Offset 6: total length, u16 little-endian = 336
- Offset 8: revision, u32 little-endian
- Offset 12: VE map, then ignition map; each map has 8 RPM axis u16, 8 load axis u16 and 64 signed cell values i16 in row-major order
- Final four bytes: IEEE CRC32, little-endian, covering all preceding bytes

A map is 160 bytes; two maps are 320 bytes. Thus the intended total length is **336 bytes** (12 + 320 + 4). The serializer and tests use this exact 336-byte length.

Chunks are capped at 32 bytes. The format is not yet exposed over TunerStudio or Console serial commands. It is RAM-only, read-only, and does not modify ECU outputs.
