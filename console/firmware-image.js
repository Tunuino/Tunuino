// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
// Offline validation only. Never interprets a BIN as permission to flash an ECU.
export const F429_FLASH_BASE = 0x08000000;
export const F429_FLASH_SIZE = 2 * 1024 * 1024;
export const F429_RAM_BASE = 0x20000000;
export const F429_RAM_END = 0x20030000;
export function inspectFirmwareImage(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length < 8 || bytes.length > F429_FLASH_SIZE)
    throw new Error("Tamanho de firmware inválido para STM32F429ZI");
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const stack = view.getUint32(0, true);
  const reset = view.getUint32(4, true);
  if ((stack & 3) || stack <= F429_RAM_BASE || stack > F429_RAM_END)
    throw new Error("Vetor de stack fora da RAM STM32F429");
  if (!(reset & 1) || (reset & ~1) < F429_FLASH_BASE ||
      (reset & ~1) >= F429_FLASH_BASE + bytes.length)
    throw new Error("Vetor de arranque inválido para imagem Flash STM32F429");
  return {size: bytes.length, stack, reset, base: F429_FLASH_BASE};
}
