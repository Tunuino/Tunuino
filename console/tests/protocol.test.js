// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import test from "node:test";
import assert from "node:assert/strict";
import {crc16, encodeFrame, decodeFrame, decodeTelemetry, TYPE_TELEMETRY} from "../protocol.js";
test("standard CRC16 CCITT-FALSE vector", () => {
  assert.equal(crc16(new TextEncoder().encode("123456789")), 0x29b1);
});
test("frame round trip and corrupted frame rejection", () => {
  const payload = Uint8Array.from([0, 1, 255, 32]);
  const frame = encodeFrame(TYPE_TELEMETRY, payload);
  assert.equal(decodeFrame(frame).type, TYPE_TELEMETRY);
  assert.deepEqual(decodeFrame(frame).payload, payload);
  const corrupt = frame.slice(); corrupt[6] ^= 1;
  assert.throws(() => decodeFrame(corrupt), /CRC/);
  assert.throws(() => decodeFrame(frame.slice(0, -1)), /length/);
  assert.throws(() => encodeFrame(1, new Uint8Array(65)), RangeError);
});
test("telemetry decode", () => {
  const raw = new Uint8Array(14);
  const view = new DataView(raw.buffer);
  [2500, 1013, 275, 1250, 700, 1470, 1380].forEach((n, i) => view.setUint16(i * 2, n, true));
  assert.deepEqual(decodeTelemetry(raw), {rpm: 2500, map: 101.3, tps: 27.5, clt: 85, iat: 30, afr: 14.7, battery: 13.8});
});
