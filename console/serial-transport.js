// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import {TelemetryStream} from "./telemetry-stream.js";
import {TYPE_TUNE_CHUNK} from "./tune-snapshot.js";
import {makeTuneReadRequest} from "./tune-request.js";
import {TUNE_SIZE, TUNE_CHUNK_MAX} from "./tune-snapshot.js";
import {decodeFrame, PROTOCOL_VERSION, TYPE_HELLO, MAX_PAYLOAD} from "./protocol.js";
// Read-only Web Serial transport. Does not send data or control the ECU.
export class TunuinoSerialTransport {
  constructor({onTelemetry, onStatus, onTuneChunk}) {
    this.onTuneChunk = onTuneChunk ?? (() => {});
    this.onStatus = onStatus ?? (() => {});
    this.telemetry = new TelemetryStream(onTelemetry);
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.pendingTune = null;
    this.tuneReading = false;
    this.tuneError = null;
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
      this.#clearPendingTune();
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
  async requestTune({timeoutMs = 1200, retries = 2} = {}) {
    if (!this.connected || !this.identified || !this.running || !this.port?.writable)
      throw new Error("ECU não identificada ou porta indisponível");
    if (this.tuneReading) throw new Error("Leitura já em curso");
    this.tuneReading = true;
    this.tuneError = null;
    const writer = this.port.writable.getWriter();
    this.writer = writer;
    try {
      for (let offset = 0; offset < TUNE_SIZE; offset += TUNE_CHUNK_MAX) {
        const count = Math.min(TUNE_CHUNK_MAX, TUNE_SIZE - offset);
        let received = false;
        for (let attempt = 0; attempt <= retries && !received; attempt++) {
          if (!this.running || !this.identified) throw new Error("Ligação interrompida");
          if (this.tuneError) throw this.tuneError;
          const acknowledgement = new Promise(resolve => {
            const pending = {offset, count, resolve, timer: null};
            pending.timer = setTimeout(() => {
              if (this.pendingTune === pending) this.pendingTune = null;
              resolve(false);
            }, timeoutMs);
            this.pendingTune = pending;
          });
          try {
            await writer.write(makeTuneReadRequest(offset, count));
            received = await acknowledgement;
            if (this.tuneError) throw this.tuneError;
          } catch (error) {
            this.#clearPendingTune();
            throw error;
          }
        }
        if (!received) throw new Error("Sem resposta da ECU no bloco " + offset);
      }
    } finally {
      this.#clearPendingTune();
      writer.releaseLock();
      this.writer = null;
      this.tuneReading = false;
    }
  }
  #clearPendingTune() {
    const pending = this.pendingTune;
    if (!pending) return;
    this.pendingTune = null;
    clearTimeout(pending.timer);
    pending.resolve(false);
  }
  async disconnect() {
    this.running = false;
    this.#clearPendingTune();
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
          if (frame.type === TYPE_TUNE_CHUNK) {
            const pending = this.pendingTune;
            const payload = frame.payload;
            if (pending && payload.length === pending.count + 3 &&
                (payload[0] | (payload[1] << 8)) === pending.offset &&
                payload[2] === pending.count) {
              try { this.onTuneChunk(payload); }
              catch (error) { this.tuneError = error; }
              this.pendingTune = null;
              clearTimeout(pending.timer);
              pending.resolve(!this.tuneError);
            }
          }
          else this.telemetry.push(frameBytes);
        }
      } catch { this.buffer.shift(); }
    }
  }
}
