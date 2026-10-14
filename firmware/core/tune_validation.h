// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TUNE_VALIDATION_H
#define TUNUINO_TUNE_VALIDATION_H
#include <stdint.h>
#include "tune_maps.h"
// Data integrity limits only; these are not a safe engine calibration.
typedef enum {
    TUNUINO_TUNE_OK = 0,
    TUNUINO_TUNE_BAD_AXIS = 1,
    TUNUINO_TUNE_BAD_RPM = 2,
    TUNUINO_TUNE_BAD_LOAD = 3,
    TUNUINO_TUNE_BAD_VE = 4,
    TUNUINO_TUNE_BAD_IGNITION = 5
} tunuino_tune_error_t;
tunuino_tune_error_t tunuino_tune_validate(const tunuino_tune_maps_t *maps);
#endif
