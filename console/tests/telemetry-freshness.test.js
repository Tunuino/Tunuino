// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import test from "node:test";
import assert from "node:assert/strict";
import {TelemetryFreshness} from "../telemetry-freshness.js";
test("stale telemetry expires exactly once until refreshed", () => {
  let expired = 0;
  const f = new TelemetryFreshness(() => expired++, 500);
  f.received(1000);
  assert.equal(f.check(1500), false);
  assert.equal(f.check(1501), true);
  assert.equal(f.check(2000), false);
  assert.equal(expired, 1);
  f.received(2100);
  assert.equal(f.check(2700), true);
  assert.equal(expired, 2);
  f.reset();
  assert.equal(f.check(4000), false);
});
