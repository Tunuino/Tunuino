// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "../flash_regions.h"
#include <assert.h>
int main(void) {
  assert(tunuino_flash_sector_for_address(0x08000000u) == 0);
  assert(tunuino_flash_sector_for_address(0x08008000u) == 2);
  assert(tunuino_flash_sector_for_address(0x08100000u) == 12);
  assert(tunuino_flash_sector_for_address(0x081FFFFFu) == 23);
  assert(tunuino_flash_sector_for_address(0x08200000u) == -1);
  assert(tunuino_flash_sector_for_address(0x07FFFFFFu) == -1);
  assert(!tunuino_flash_erase_allowed(0, 0x08008000u, 0x08200000u));
  assert(tunuino_flash_erase_allowed(2, 0x08008000u, 0x08200000u));
  assert(!tunuino_flash_erase_allowed(2, 0x08009000u, 0x08200000u));
  assert(!tunuino_flash_erase_allowed(24, 0x08008000u, 0x08200000u));
  assert(!tunuino_flash_erase_allowed(23, 0x08008000u, 0x08100000u));
  return 0;
}
