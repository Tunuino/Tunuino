// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "tune_store.h"
#include <string.h>
tunuino_tune_error_t tunuino_tune_store_init(tunuino_tune_store_t *store,
    const tunuino_tune_maps_t *candidate) {
    if (!store) return TUNUINO_TUNE_BAD_AXIS;
    // Fail closed and invalidate any previous state before accepting a candidate.
    store->valid = 0u;
    const tunuino_tune_error_t status = tunuino_tune_validate(candidate);
    if (status != TUNUINO_TUNE_OK) return status;
    memcpy(&store->maps, candidate, sizeof(store->maps));
    store->revision = 1u;
    store->valid = 1u;
    return TUNUINO_TUNE_OK;
}
uint8_t tunuino_tune_store_snapshot(const tunuino_tune_store_t *store,
    tunuino_tune_maps_t *out, uint32_t *revision) {
    if (!store || !store->valid || !out) return 0u;
    memcpy(out, &store->maps, sizeof(*out));
    if (revision) *revision = store->revision;
    return 1u;
}
uint8_t tunuino_tune_store_lookup(const tunuino_tune_store_t *store,
    uint8_t ignition, uint16_t rpm, uint16_t load_tenths_kpa, int16_t *out) {
    if (!store || !store->valid || ignition > 1u) return 0u;
    return tunuino_tune_map_lookup(ignition ? &store->maps.ignition : &store->maps.ve,
        rpm, load_tenths_kpa, out);
}
