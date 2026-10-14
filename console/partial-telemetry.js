// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
export const TYPE_PARTIAL_TELEMETRY = 3;
export const FIELD_NAMES = ["rpm", "map", "tps", "clt", "iat", "afr", "battery"];
export function decodePartialTelemetry(payload) {
  if (!(payload instanceof Uint8Array) || payload.length !== 16)
    throw new Error("Invalid partial telemetry length");
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  const mask = view.getUint16(0, true);
  if ((mask & ~0x7f) !== 0) throw new Error("Invalid validity mask");
  const raw = Array.from({length: 7}, (_, i) => view.getUint16(2 + 2 * i, true));
  const converted = [raw[0], raw[1] / 10, raw[2] / 10, raw[3] / 10 - 40,
    raw[4] / 10 - 40, raw[5] / 100, raw[6] / 100];
  return Object.fromEntries(FIELD_NAMES.map((name, i) =>
    [name, (mask & (1 << i)) ? converted[i] : null]));
}
