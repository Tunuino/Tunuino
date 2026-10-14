// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import test from "node:test";
import assert from "node:assert/strict";
import {TunuinoSerialTransport} from "../serial-transport.js";
import {encodeFrame, TYPE_HELLO, TYPE_TELEMETRY} from "../protocol.js";
test("requires identification before telemetry", async () => {
  const values = [], states = [];
  const t = new TunuinoSerialTransport({onTelemetry: v => values.push(v), onStatus: s => states.push(s)});
  const payload = new Uint8Array(14);
  new DataView(payload.buffer).setUint16(0, 1200, true);
  const telemetry = encodeFrame(TYPE_TELEMETRY, payload);
  const hello = encodeFrame(TYPE_HELLO, Uint8Array.from([0x54, 0x4e, 1]));
  const chunks = [telemetry, hello.subarray(0, 5), hello.subarray(5), telemetry];
  let index = 0;
  const reader = {read: async () => index < chunks.length ? {value: chunks[index++], done: false} : {done: true},
    releaseLock() {}, cancel: async () => {}};
  const port = {open: async () => {}, readable: {getReader: () => reader}, close: async () => {}};
  await t.connect({requestPort: async () => port});
  assert.equal(values.length, 1);
  assert.equal(values[0].rpm, 1200);
  assert.ok(states.includes("identified"));
  assert.equal(t.connected, false);
});
test("unavailable serial is rejected", async () => {
  const t = new TunuinoSerialTransport({onTelemetry: () => {}});
  await assert.rejects(t.connect({}), /unavailable/);
});
