// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import test from "node:test";
import assert from "node:assert/strict";
import {makeDemoMap, setCell, mapToCsv, rpmAxis, loadAxis} from "../map-model.js";
test("demo map dimensions and export", () => {
  const map = makeDemoMap();
  assert.equal(map.length, loadAxis.length);
  assert.equal(map[0].length, rpmAxis.length);
  assert.equal(mapToCsv(map).trim().split("\r\n").length, loadAxis.length + 1);
});
test("editing validates ranges", () => {
  const map = makeDemoMap("ignition");
  setCell(map, 0, 0, "12.5", "ignition");
  assert.equal(map[0][0], 12.5);
  assert.throws(() => setCell(map, 0, 0, "nan", "ignition"), RangeError);
  assert.throws(() => setCell(map, 0, 0, 200, "ignition"), RangeError);
  assert.throws(() => setCell(map, 100, 0, 10), RangeError);
});
