// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import test from "node:test";
import assert from "node:assert/strict";
import {decodePartialTelemetry} from "../partial-telemetry.js";
test("RPM-only frame does not fabricate missing sensor readings", () => {
  const p = new Uint8Array(16);
  const view = new DataView(p.buffer);
  view.setUint16(0, 1, true);
  view.setUint16(2, 1850, true);
  const values = decodePartialTelemetry(p);
  assert.equal(values.rpm, 1850);
  for (const key of ["map", "tps", "clt", "iat", "afr", "battery"])
    assert.equal(values[key], null);
});
test("invalid masks and lengths are rejected", () => {
  assert.throws(() => decodePartialTelemetry(new Uint8Array(15)));
  const p = new Uint8Array(16);
  p[0] = 0x80;
  assert.throws(() => decodePartialTelemetry(p));
});
