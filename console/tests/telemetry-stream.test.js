// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import test from "node:test";
import assert from "node:assert/strict";
import {TelemetryStream} from "../telemetry-stream.js";
import {encodeFrame, TYPE_TELEMETRY} from "../protocol.js";
function telemetryFrame() {
  const payload = new Uint8Array(14);
  new DataView(payload.buffer).setUint16(0, 3250, true);
  return encodeFrame(TYPE_TELEMETRY, payload);
}
test("accepts fragmented messages and noise", () => {
  const received = [];
  const parser = new TelemetryStream(t => received.push(t));
  const frame = telemetryFrame();
  parser.push(Uint8Array.from([0x00, 0x54, 0x00]));
  parser.push(frame.subarray(0, 4));
  assert.equal(received.length, 0);
  parser.push(frame.subarray(4));
  assert.equal(received.length, 1);
  assert.equal(received[0].rpm, 3250);
  assert.equal(parser.validFrames, 1);
});
test("rejects bad CRC and recovers on next frame", () => {
  const received = [];
  const parser = new TelemetryStream(t => received.push(t));
  const bad = telemetryFrame(); bad[6] ^= 1;
  const good = telemetryFrame();
  parser.push(Uint8Array.from([...bad, ...good]));
  assert.equal(received.length, 1);
  assert.equal(parser.validFrames, 1);
  assert.ok(parser.rejectedFrames >= 1);
});
test("rejects impossible payload lengths", () => {
  const parser = new TelemetryStream(() => {});
  parser.push(Uint8Array.from([0x54, 0x4e, 1, 2, 0xff, 0xff]));
  assert.equal(parser.rejectedFrames, 1);
});
