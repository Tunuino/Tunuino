// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import {TelemetryStream} from "./telemetry-stream.js";
import {TYPE_TUNE_CHUNK} from "./tune-snapshot.js";
import {decodeFrame, PROTOCOL_VERSION, TYPE_HELLO, MAX_PAYLOAD} from "./protocol.js";
// Read-only Web Serial transport. Does not send data or control the ECU.
export class TunuinoSerialTransport {
  constructor({onTelemetry, onStatus, onTuneChunk}) {
    this.onTuneChunk = onTuneChunk ?? (() => {});
    this.onStatus = onStatus ?? (() => {});
    this.telemetry = new TelemetryStream(onTelemetry);
    this.port = null;
    this.reader = null;
    this.connected = false;
    this.identified = false;
    this.running = false;
    this.buffer = [];
  }
  async connect(serial = globalThis.navigator?.serial) {
    if (this.running) throw new Error("Already connecting");
    if (!serial?.requestPort) throw new Error("Web Serial unavailable");
    this.running = true;
    try {
      this.port = await serial.requestPort();
      await this.port.open({baudRate: 115200});
      this.connected = true;
      this.onStatus("serial-open");
      this.reader = this.port.readable.getReader();
      while (this.running) {
        const {value, done} = await this.reader.read();
        if (done) break;
        if (value) this.#receive(value);
      }
    } catch (error) {
      this.onStatus("error");
      throw error;
    } finally {
      this.running = false;
      this.connected = false;
      this.identified = false;
      if (this.reader) {
        try { this.reader.releaseLock(); } catch {}
        this.reader = null;
      }
      if (this.port) {
        try { await this.port.close(); } catch {}
        this.port = null;
      }
      this.onStatus("disconnected");
    }
  }
  async disconnect() {
    this.running = false;
    if (this.reader) await this.reader.cancel();
  }
  #receive(chunk) {
    // Identification and telemetry share the same stream. Never publish
    // telemetry before a valid Tunuino HELLO frame is received.
    for (const byte of chunk) {
      this.buffer.push(byte);
      while (this.buffer.length >= 2 && (this.buffer[0] !== 0x54 || this.buffer[1] !== 0x4e))
        this.buffer.shift();
      if (this.buffer.length < 6) continue;
      const length = this.buffer[4] | (this.buffer[5] << 8);
      if (length > MAX_PAYLOAD) { this.buffer.shift(); continue; }
      if (this.buffer.length < length + 8) continue;
      const frameBytes = Uint8Array.from(this.buffer.slice(0, length + 8));
      try {
        const frame = decodeFrame(frameBytes);
        this.buffer.splice(0, length + 8);
        if (frame.type === TYPE_HELLO && frame.payload.length === 3 &&
            frame.payload[0] === 0x54 && frame.payload[1] === 0x4e &&
            frame.payload[2] === PROTOCOL_VERSION) {
          this.identified = true;
          this.onStatus("identified");
        } else if (this.identified) {
          if (frame.type === TYPE_TUNE_CHUNK) this.onTuneChunk(frame.payload);
          else this.telemetry.push(frameBytes);
        }
      } catch { this.buffer.shift(); }
    }
  }
}
