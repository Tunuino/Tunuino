// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
// Tunuino protocol v1: framed, read-only telemetry. All multi-byte integers little-endian.
// Frame: 0x54 0x4E | version u8 | type u8 | payload_length u16 | payload | CRC16-CCITT-FALSE u16.
// CRC covers version through payload, excludes sync bytes and CRC.
export const PROTOCOL_VERSION = 1;
export const TYPE_HELLO = 1;
export const TYPE_TELEMETRY = 2;
export const MAX_PAYLOAD = 64;
export function crc16(bytes) {
  let crc = 0xffff;
  for (const byte of bytes) {
    crc ^= byte << 8;
    for (let i = 0; i < 8; i++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
  }
  return crc;
}
export function encodeFrame(type, payload = new Uint8Array()) {
  if (!(payload instanceof Uint8Array) || payload.length > MAX_PAYLOAD) throw new RangeError("Invalid payload");
  if (!Number.isInteger(type) || type < 0 || type > 255) throw new RangeError("Invalid type");
  const out = new Uint8Array(payload.length + 8);
  out[0] = 0x54; out[1] = 0x4e; out[2] = PROTOCOL_VERSION; out[3] = type;
  out[4] = payload.length & 255; out[5] = payload.length >>> 8;
  out.set(payload, 6);
  const crc = crc16(out.subarray(2, 6 + payload.length));
  out[6 + payload.length] = crc & 255;
  out[7 + payload.length] = crc >>> 8;
  return out;
}
export function decodeFrame(frame) {
  if (!(frame instanceof Uint8Array) || frame.length < 8 ||
      frame[0] !== 0x54 || frame[1] !== 0x4e) throw new Error("Invalid sync");
  if (frame[2] !== PROTOCOL_VERSION) throw new Error("Unsupported version");
  const length = frame[4] | (frame[5] << 8);
  if (length > MAX_PAYLOAD || frame.length !== length + 8) throw new Error("Invalid length");
  const actual = frame[6 + length] | (frame[7 + length] << 8);
  if (actual !== crc16(frame.subarray(2, 6 + length))) throw new Error("CRC mismatch");
  return {type: frame[3], payload: frame.slice(6, 6 + length)};
}
// Telemetry payload v1: seven unsigned 16-bit values, scale factors documented below.
// rpm x1, MAP x0.1 kPa, TPS x0.1%, CLT/IAT x0.1 C with +40 C offset,
// AFR x0.01, battery x0.01 V. All are unsigned.
export function decodeTelemetry(payload) {
  if (!(payload instanceof Uint8Array) || payload.length !== 14) throw new Error("Invalid telemetry length");
  const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  const u = i => view.getUint16(i * 2, true);
  return {rpm: u(0), map: u(1) / 10, tps: u(2) / 10,
    clt: u(3) / 10 - 40, iat: u(4) / 10 - 40,
    afr: u(5) / 100, battery: u(6) / 100};
}
