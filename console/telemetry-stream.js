// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import {decodeFrame, decodeTelemetry, TYPE_TELEMETRY, MAX_PAYLOAD} from "./protocol.js";
import {decodePartialTelemetry, TYPE_PARTIAL_TELEMETRY} from "./partial-telemetry.js";
// Stateful parser for arbitrary serial chunk boundaries. Read-only.
// Buffer is bounded to the maximum frame size; invalid frames resync by one byte.
export class TelemetryStream {
  constructor(onTelemetry) {
    if (typeof onTelemetry !== "function") throw new TypeError("callback required");
    this.onTelemetry = onTelemetry;
    this.buffer = [];
    this.validFrames = 0;
    this.rejectedFrames = 0;
  }
  push(bytes) {
    if (!(bytes instanceof Uint8Array)) throw new TypeError("Uint8Array required");
    for (const byte of bytes) {
      this.buffer.push(byte);
      this.#process();
    }
  }
  #process() {
    while (this.buffer.length >= 2) {
      if (this.buffer[0] !== 0x54 || this.buffer[1] !== 0x4e) {
        this.buffer.shift();
        continue;
      }
      if (this.buffer.length < 6) return;
      const size = this.buffer[4] | (this.buffer[5] << 8);
      if (size > MAX_PAYLOAD) {
        this.rejectedFrames++;
        this.buffer.shift();
        continue;
      }
      const total = size + 8;
      if (this.buffer.length < total) return;
      try {
        const frame = decodeFrame(Uint8Array.from(this.buffer.slice(0, total)));
        if (frame.type === TYPE_TELEMETRY) {
          const telemetry = decodeTelemetry(frame.payload);
          this.onTelemetry(telemetry);
          this.validFrames++;
        } else if (frame.type === TYPE_PARTIAL_TELEMETRY) {
          this.onTelemetry(decodePartialTelemetry(frame.payload));
          this.validFrames++;
        }
        this.buffer.splice(0, total);
      } catch {
        this.rejectedFrames++;
        this.buffer.shift();
      }
    }
  }
}
