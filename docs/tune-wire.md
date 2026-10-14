# Read-only tune snapshot wire format, v1

The validated tune store can be serialized into a deterministic 1168-byte snapshot. The layout is:

- Offset 0: ASCII `TUNE` (4 bytes)
- Offset 4: format version, u16 little-endian = 2
- Offset 6: total length, u16 little-endian = 1168
- Offset 8: revision, u32 little-endian
- Offset 12: VE map, then ignition map; each map has 16 RPM axis u16, 16 load axis u16 and 256 signed cell values i16 in row-major order
- Final four bytes: IEEE CRC32, little-endian, covering all preceding bytes

A map is 576 bytes; two maps are 1152 bytes. Total length is **1168 bytes** (12 + 1152 + 4). The serializer and tests use this exact 1168-byte length.

Chunks are capped at 32 bytes. The format is not yet exposed over TunerStudio or Console serial commands. It is RAM-only, read-only, and does not modify ECU outputs.
