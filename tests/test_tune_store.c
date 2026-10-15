// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/tune_store.h"
#include <assert.h>
int main(void) {
    tunuino_tune_maps_t maps = {0};
    for (unsigned t = 0; t < 2u; ++t) {
        tunuino_tune_map_t *m = t ? &maps.ignition : &maps.ve;
        for (unsigned i = 0; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
            m->rpm_axis[i] = (uint16_t)(1000u + i * 500u);
            m->load_axis[i] = (uint16_t)(200u + i * 100u);
            for (unsigned j = 0; j < TUNUINO_MAP_AXIS_SIZE; ++j)
                m->cells[i][j] = t ? 120 : 850;
        }
    }
    tunuino_tune_store_t store = {0};
    tunuino_tune_maps_t copy = {0};
    uint32_t revision = 0u;
    int16_t result = 0;
    assert(!tunuino_tune_store_snapshot(&store, &copy, &revision));
    assert(tunuino_tune_store_init(&store, &maps) == TUNUINO_TUNE_OK);
    assert(tunuino_tune_store_snapshot(&store, &copy, &revision));
    assert(revision == 1u && copy.ve.cells[0][0] == 850);
    maps.ve.cells[0][0] = 999;
    assert(tunuino_tune_store_lookup(&store, 0u, 1000u, 200u, &result) && result == 850);
    assert(tunuino_tune_store_lookup(&store, 1u, 1000u, 200u, &result) && result == 120);
    assert(!tunuino_tune_store_lookup(&store, 2u, 1000u, 200u, &result));
    maps.ve.cells[0][0] = -1;
    assert(tunuino_tune_store_init(&store, &maps) == TUNUINO_TUNE_BAD_VE);
    assert(!tunuino_tune_store_snapshot(&store, &copy, &revision));
    return 0;
}
