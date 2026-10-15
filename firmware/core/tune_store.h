// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TUNE_STORE_H
#define TUNUINO_TUNE_STORE_H
#include <stdint.h>
#include "tune_validation.h"
typedef struct {
    tunuino_tune_maps_t maps;
    uint32_t revision;
    uint8_t valid;
} tunuino_tune_store_t;
// Validated RAM-only initialization; no write transport and no flash operations.
tunuino_tune_error_t tunuino_tune_store_init(tunuino_tune_store_t *store,
    const tunuino_tune_maps_t *candidate);
// Copies an immutable snapshot to the caller. Returns zero if unavailable.
uint8_t tunuino_tune_store_snapshot(const tunuino_tune_store_t *store,
    tunuino_tune_maps_t *out, uint32_t *revision);
// Lookup on the common validated map set; ignition=0 selects VE, 1 selects ignition.
uint8_t tunuino_tune_store_lookup(const tunuino_tune_store_t *store,
    uint8_t ignition, uint16_t rpm, uint16_t load_tenths_kpa, int16_t *out);
#endif
