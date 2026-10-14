# Console tune chunk response (experimental)

Type 4 frames are read-only tune chunk responses. The request helper takes three payload bytes: offset u16 little-endian and count u8, 1..32. A valid response has the same offset and count followed by the requested bytes from the 1168-byte versioned tune snapshot, framed with Console protocol v1 and CRC16.

The whole tune snapshot carries its own CRC32. A client should verify that CRC32 after collecting all chunks. The helper does **not** yet implement an incoming serial frame parser or a browser-side download command. It must only be invoked after a validated request is received by a future command dispatcher.

No TunerStudio read command is implied by this frame type; TunerStudio remains a separate experimental protocol. No writes or hardware outputs are enabled.
