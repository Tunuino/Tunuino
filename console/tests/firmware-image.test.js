// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
import test from "node:test";
import assert from "node:assert/strict";
import {inspectFirmwareImage} from "../firmware-image.js";
function image(stack = 0x20020000, reset = 0x08000009) {
  const bytes = new Uint8Array(128);
  const view = new DataView(bytes.buffer);
  view.setUint32(0, stack, true);
  view.setUint32(4, reset, true);
  return bytes;
}
test("accepts plausible STM32F429 raw BIN vectors", () => {
  assert.equal(inspectFirmwareImage(image()).size, 128);
});
test("rejects wrong stack and non-Thumb reset vector", () => {
  assert.throws(() => inspectFirmwareImage(image(0x10000000)), /stack/);
  assert.throws(() => inspectFirmwareImage(image(0x20020000, 0x08000008)), /arranque/);
  assert.throws(() => inspectFirmwareImage(image(0x20020000, 0x08010001)), /arranque/);
});
test("rejects empty and oversized images", () => {
  assert.throws(() => inspectFirmwareImage(new Uint8Array(0)), /Tamanho/);
  assert.throws(() => inspectFirmwareImage(new Uint8Array(2097153)), /Tamanho/);
});

test("bootloader-offset image must match its exact application origin", () => {
  const app = image(0x20020000, 0x08008009);
  assert.equal(inspectFirmwareImage(app, {base: 0x08008000}).base, 0x08008000);
  assert.throws(() => inspectFirmwareImage(app), /arranque/);
  assert.throws(() => inspectFirmwareImage(image(), {base: 0x08008000}), /arranque/);
  assert.throws(() => inspectFirmwareImage(app, {base: 0x08008001}), /Endereço/);
});
