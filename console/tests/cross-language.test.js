// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import test from "node:test";
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import {decodeFrame, decodeTelemetry, encodeFrame, TYPE_HELLO, TYPE_TELEMETRY} from "../protocol.js";
const path = new URL("../../tests/console_protocol_vectors.txt", import.meta.url);
const vectors = Object.fromEntries(readFileSync(path, "utf8").trim().split(/\r?\n/).map(line => line.split(" ")));
test("C firmware frames match JS encoding and decoding", () => {
  const hello = Uint8Array.from(Buffer.from(vectors.hello, "hex"));
  const h = decodeFrame(hello);
  assert.equal(h.type, TYPE_HELLO);
  assert.deepEqual([...h.payload], [0x54, 0x4e, 1]);
  assert.deepEqual([...encodeFrame(TYPE_HELLO, h.payload)], [...hello]);
  const telemetry = Uint8Array.from(Buffer.from(vectors.telemetry, "hex"));
  const t = decodeFrame(telemetry);
  assert.equal(t.type, TYPE_TELEMETRY);
  assert.deepEqual(decodeTelemetry(t.payload), {
    rpm: 1500, map: 101.3, tps: 32.5, clt: 85, iat: 25,
    afr: 14.7, battery: 13.8
  });
  assert.deepEqual([...encodeFrame(TYPE_TELEMETRY, t.payload)], [...telemetry]);
});
