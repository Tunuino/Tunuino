// SPDX-License-Identifier: MIT
import test from "node:test";
import assert from "node:assert/strict";
import {encodeOpenBltPacket, OpenBltPacketDecoder} from "../openblt-framing.js";
test("encodes OpenBLT serial length-prefixed packets", () => {
  assert.deepEqual([...encodeOpenBltPacket(Uint8Array.of(0x10, 0x20))], [2, 0x10, 0x20]);
});
test("decodes fragmented and consecutive packets", () => {
  const decoder = new OpenBltPacketDecoder();
  assert.deepEqual(decoder.push(Uint8Array.of(2, 0x10)), []);
  assert.deepEqual(decoder.push(Uint8Array.of(0x20, 1, 0x30)).map(p => [...p]), [[0x10, 0x20], [0x30]]);
});
test("rejects invalid lengths and resets", () => {
  const decoder = new OpenBltPacketDecoder({maxLength: 3});
  assert.throws(() => decoder.push(Uint8Array.of(4)), /Comprimento/);
  assert.deepEqual(decoder.push(Uint8Array.of(1, 9)).map(p => [...p]), [[9]]);
  assert.throws(() => encodeOpenBltPacket(new Uint8Array(0)), /inválido/);
  assert.throws(() => encodeOpenBltPacket(new Uint8Array(256)), /inválido/);
});
