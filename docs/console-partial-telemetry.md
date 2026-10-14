# Console partial telemetry extension

Frame type 3 extends protocol v1 without changing the legacy type 2 frame. Payload length is 16 bytes: `valid_mask` u16 little-endian, followed by the same seven u16 telemetry fields in legacy order.

Bits 0..6 correspond to RPM, MAP, TPS, CLT, IAT, AFR and battery. Bits 7..15 are reserved and must be zero. A cleared bit means **unavailable**, not zero. Encoders write zero into unavailable fields, but receivers must ignore those bytes. A set bit means the producer claims the reading is valid; acquisition-layer freshness and physical plausibility remain its responsibility.

Legacy type 2 frames remain supported. Type 3 permits RPM-only data and valid engine-off RPM=0. No sensor hardware, GPIO, UART or engine outputs are enabled by this extension.
