// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "../boot_decision.h"
#include <assert.h>
int main(void) {
  const uint32_t app = UINT32_C(0x08008000);
  const uint32_t sp = UINT32_C(0x20010000);
  const uint32_t reset = app + 0x101;
  assert(tunuino_boot_decide(0, 1, sp, reset, app, 4096) == TUNUINO_BOOT_START_APPLICATION);
  assert(tunuino_boot_decide(1, 1, sp, reset, app, 4096) == TUNUINO_BOOT_STAY);
  assert(tunuino_boot_decide(0, 0, sp, reset, app, 4096) == TUNUINO_BOOT_STAY);
  assert(tunuino_boot_decide(0, 1, 0, reset, app, 4096) == TUNUINO_BOOT_STAY);
  assert(tunuino_boot_decide(0, 1, sp, reset & ~1u, app, 4096) == TUNUINO_BOOT_STAY);
  return 0;
}
