// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_BOOT_DECISION_H
#define TUNUINO_BOOT_DECISION_H
#include "flash_layout.h"
typedef enum {
  TUNUINO_BOOT_STAY = 0,
  TUNUINO_BOOT_START_APPLICATION = 1
} tunuino_boot_action_t;
// Pure decision logic. No peripheral writes or unsafe jump operations.
// Caller must independently verify the complete firmware and recovery path.
static inline tunuino_boot_action_t tunuino_boot_decide(
    int recovery_requested, int firmware_verified, uint32_t sp,
    uint32_t reset, uint32_t app_start, uint32_t image_size) {
  if (recovery_requested || !firmware_verified ||
      !tunuino_valid_application_vectors(sp, reset, app_start, image_size))
    return TUNUINO_BOOT_STAY;
  return TUNUINO_BOOT_START_APPLICATION;
}
#endif
