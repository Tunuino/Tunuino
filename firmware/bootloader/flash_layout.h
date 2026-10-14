// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_BOOT_FLASH_LAYOUT_H
#define TUNUINO_BOOT_FLASH_LAYOUT_H
#include <stdint.h>
// STM32F429 2 MiB dual-bank Flash, 24 sectors.
// Application offset is intentionally NOT fixed until hardware bootloader
// configuration and sector reservation are verified.
#define TUNUINO_FLASH_START UINT32_C(0x08000000)
#define TUNUINO_FLASH_END   UINT32_C(0x08200000)
#define TUNUINO_SRAM_START  UINT32_C(0x20000000)
#define TUNUINO_SRAM_END    UINT32_C(0x20030000)
static inline int tunuino_valid_application_vectors(uint32_t sp, uint32_t reset,
                                                     uint32_t app_start, uint32_t image_size) {
  if (app_start < TUNUINO_FLASH_START || app_start >= TUNUINO_FLASH_END ||
      (app_start & 511u) != 0 || image_size < 8 ||
      image_size > TUNUINO_FLASH_END - app_start) return 0;
  return sp > TUNUINO_SRAM_START && sp <= TUNUINO_SRAM_END &&
         (sp & 7u) == 0 && (reset & 1u) != 0 &&
         (reset & ~UINT32_C(1)) >= app_start &&
         (reset & ~UINT32_C(1)) < app_start + image_size;
}
#endif
