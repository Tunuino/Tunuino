// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/tune_maps.h"
#include <assert.h>
int main(void) {
    tunuino_tune_map_t m = {0};
    for (unsigned i = 0; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
        m.rpm_axis[i] = (uint16_t)(1000u + i * 500u);
        m.load_axis[i] = (uint16_t)(200u + i * 100u);
        for (unsigned j = 0; j < TUNUINO_MAP_AXIS_SIZE; ++j)
            m.cells[i][j] = (int16_t)(i * 100 + j * 200);
    }
    int16_t out = -123;
    assert(tunuino_tune_map_valid(&m));
    assert(tunuino_tune_map_lookup(&m, 1000u, 200u, &out) && out == 0);
    assert(tunuino_tune_map_lookup(&m, 1250u, 250u, &out) && out == 150);
    assert(tunuino_tune_map_lookup(&m, 16000u, 3200u, &out) && out == 4500);
    assert(tunuino_tune_map_lookup(&m, 0u, 0u, &out) && out == 0);
    assert(tunuino_tune_map_lookup(&m, 1500u, 300u, &out) && out == 300);
    m.rpm_axis[1] = m.rpm_axis[0];
    out = 777;
    assert(!tunuino_tune_map_lookup(&m, 1500u, 300u, &out) && out == 777);
    assert(!tunuino_tune_map_lookup(0, 0u, 0u, &out));
    assert(!tunuino_tune_map_lookup(&m, 0u, 0u, 0));
    return 0;
}
