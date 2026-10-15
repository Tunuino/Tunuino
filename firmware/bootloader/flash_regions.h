// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_FLASH_REGIONS_H
#define TUNUINO_FLASH_REGIONS_H
#include "flash_layout.h"
#include <stddef.h>
// STM32F429 2 MiB flash: 16K x4, 64K x1, 128K x7 per bank.
// This is a descriptive map, NOT a flash erase/program driver.
static const uint32_t tunuino_flash_sector_bounds[] = {
  0x08000000u, 0x08004000u, 0x08008000u, 0x0800C000u,
  0x08010000u, 0x08020000u, 0x08040000u, 0x08060000u,
  0x08080000u, 0x080A0000u, 0x080C0000u, 0x080E0000u,
  0x08100000u, 0x08104000u, 0x08108000u, 0x0810C000u,
  0x08110000u, 0x08120000u, 0x08140000u, 0x08160000u,
  0x08180000u, 0x081A0000u, 0x081C0000u, 0x081E0000u,
  0x08200000u
};
#define TUNUINO_FLASH_SECTOR_COUNT 24u
static inline int tunuino_flash_sector_for_address(uint32_t address) {
  for (unsigned i = 0; i < TUNUINO_FLASH_SECTOR_COUNT; ++i)
    if (address >= tunuino_flash_sector_bounds[i] &&
        address < tunuino_flash_sector_bounds[i + 1]) return (int)i;
  return -1;
}
// Only permit erasing complete sectors inside an explicitly configured
// application region. Never infer that region from a firmware image.
static inline int tunuino_flash_erase_allowed(unsigned sector,
                                               uint32_t app_start,
                                               uint32_t app_end) {
  if (sector >= TUNUINO_FLASH_SECTOR_COUNT ||
      app_start < TUNUINO_FLASH_START || app_end > TUNUINO_FLASH_END ||
      app_start >= app_end) return 0;
  return tunuino_flash_sector_bounds[sector] >= app_start &&
         tunuino_flash_sector_bounds[sector + 1] <= app_end;
}
#endif
