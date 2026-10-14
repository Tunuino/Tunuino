// SPDX-License-Identifier: MIT
#include "../flash_layout.h"
#include <assert.h>
int main(void) {
  const uint32_t app = UINT32_C(0x08008000);
  assert(tunuino_valid_application_vectors(0x20010000, app + 0x101, app, 4096));
  assert(!tunuino_valid_application_vectors(0x20010001, app + 0x101, app, 4096));
  assert(!tunuino_valid_application_vectors(0x10000000, app + 0x101, app, 4096));
  assert(!tunuino_valid_application_vectors(0x20010000, app + 0x100, app, 4096));
  assert(!tunuino_valid_application_vectors(0x20010000, app - 1, app, 4096));
  assert(!tunuino_valid_application_vectors(0x20010000, app + 0x1001, app, 4096));
  assert(!tunuino_valid_application_vectors(0x20010000, app + 0x101, app + 1, 4096));
  assert(!tunuino_valid_application_vectors(0x20010000, app + 0x101, app, 0));
  assert(!tunuino_valid_application_vectors(0x20010000, app + 0x101, app, UINT32_MAX));
  return 0;
}
