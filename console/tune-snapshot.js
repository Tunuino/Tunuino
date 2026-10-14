// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
export const TUNE_SIZE = 1168;
export const TUNE_VERSION = 2;
export const TUNE_CHUNK_MAX = 32;
export const TYPE_TUNE_CHUNK = 4;
function u16(view, offset) { return view.getUint16(offset, true); }
export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let b = 0; b < 8; b++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}
export function decodeTuneSnapshot(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length !== TUNE_SIZE) throw new Error("Invalid tune size");
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (String.fromCharCode(...bytes.subarray(0, 4)) !== "TUNE" ||
      u16(view, 4) !== TUNE_VERSION || u16(view, 6) !== TUNE_SIZE) throw new Error("Unsupported tune format");
  if (view.getUint32(TUNE_SIZE - 4, true) !== crc32(bytes.subarray(0, TUNE_SIZE - 4)))
    throw new Error("Tune CRC32 mismatch");
  let offset = 12;
  const readMap = () => {
    const rpmAxis = Array.from({length: 16}, () => { const v = u16(view, offset); offset += 2; return v; });
    const loadAxis = Array.from({length: 16}, () => { const v = u16(view, offset); offset += 2; return v; });
    const cells = Array.from({length: 16}, () => Array.from({length: 16}, () => {
      const v = view.getInt16(offset, true); offset += 2; return v / 10;
    }));
    if (rpmAxis.some((v, i) => v < 100 || v > 16000 || (i && v <= rpmAxis[i - 1])) ||
        loadAxis.some((v, i) => v < 100 || v > 4000 || (i && v <= loadAxis[i - 1])))
      throw new Error("Invalid tune axes");
    return {rpmAxis, loadAxis: loadAxis.map(v => v / 10), cells};
  };
  const ve = readMap();
  const ignition = readMap();
  if (ve.cells.some(row => row.some(v => v < 0 || v > 300)) ||
      ignition.cells.some(row => row.some(v => v < -40 || v > 80)))
    throw new Error("Invalid tune values");
  return {revision: view.getUint32(8, true), ve, ignition};
}
export class TuneSnapshotAssembler {
  constructor() {
    this.bytes = new Uint8Array(TUNE_SIZE);
    this.received = new Uint8Array(TUNE_SIZE);
    this.count = 0;
  }
  addChunk(payload) {
    if (!(payload instanceof Uint8Array) || payload.length < 4) throw new Error("Invalid tune chunk");
    const offset = payload[0] | (payload[1] << 8);
    const count = payload[2];
    if (!count || count > TUNE_CHUNK_MAX || count !== payload.length - 3 ||
        offset + count > TUNE_SIZE) throw new Error("Invalid tune chunk range");
    for (let i = 0; i < count; i++) {
      const at = offset + i;
      if (this.received[at] && this.bytes[at] !== payload[i + 3]) throw new Error("Conflicting tune chunk");
    }
    for (let i = 0; i < count; i++) {
      const at = offset + i;
      if (!this.received[at]) { this.received[at] = 1; this.count++; }
      this.bytes[at] = payload[i + 3];
    }
    return this.count === TUNE_SIZE;
  }
  finish() {
    if (this.count !== TUNE_SIZE) throw new Error("Incomplete tune snapshot");
    return decodeTuneSnapshot(this.bytes);
  }
}
