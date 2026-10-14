// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
// Framing only: rusEFI's OpenBLT ChibiOS USB serial transport uses
// one length byte followed by a CTO packet. This module never flashes.
export function encodeOpenBltPacket(payload, {maxLength = 255} = {}) {
  if (!(payload instanceof Uint8Array) || payload.length < 1 ||
      payload.length > maxLength || payload.length > 255)
    throw new Error("Pacote OpenBLT inválido");
  const frame = new Uint8Array(payload.length + 1);
  frame[0] = payload.length;
  frame.set(payload, 1);
  return frame;
}
export class OpenBltPacketDecoder {
  constructor({maxLength = 255} = {}) {
    if (!Number.isInteger(maxLength) || maxLength < 1 || maxLength > 255)
      throw new Error("Limite OpenBLT inválido");
    this.maxLength = maxLength;
    this.pending = [];
    this.expected = 0;
  }
  reset() { this.pending = []; this.expected = 0; }
  push(chunk) {
    if (!(chunk instanceof Uint8Array)) throw new Error("Dados OpenBLT inválidos");
    const packets = [];
    for (const byte of chunk) {
      if (!this.expected) {
        if (!byte || byte > this.maxLength) {
          this.reset();
          throw new Error("Comprimento de pacote OpenBLT inválido");
        }
        this.expected = byte;
      } else {
        this.pending.push(byte);
        if (this.pending.length === this.expected) {
          packets.push(Uint8Array.from(this.pending));
          this.reset();
        }
      }
    }
    return packets;
  }
}
