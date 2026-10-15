// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/tune_validation.h"
#include <assert.h>
int main(void) {
    tunuino_tune_maps_t maps = {0};
    for (unsigned t = 0u; t < 2u; ++t) {
        tunuino_tune_map_t *m = t == 0u ? &maps.ve : &maps.ignition;
        for (unsigned i = 0u; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
            m->rpm_axis[i] = (uint16_t)(500u + 1000u * i);
            m->load_axis[i] = (uint16_t)(200u + 200u * i);
            for (unsigned j = 0u; j < TUNUINO_MAP_AXIS_SIZE; ++j)
                m->cells[i][j] = t == 0u ? 850 : 120;
        }
    }
    assert(tunuino_tune_validate(&maps) == TUNUINO_TUNE_OK);
    maps.ve.cells[3][4] = 3001;
    assert(tunuino_tune_validate(&maps) == TUNUINO_TUNE_BAD_VE);
    maps.ve.cells[3][4] = 850;
    maps.ignition.cells[0][0] = -401;
    assert(tunuino_tune_validate(&maps) == TUNUINO_TUNE_BAD_IGNITION);
    maps.ignition.cells[0][0] = 120;
    maps.ve.load_axis[0] = 0u;
    assert(tunuino_tune_validate(&maps) == TUNUINO_TUNE_BAD_LOAD);
    maps.ve.load_axis[0] = 200u;
    maps.ignition.rpm_axis[1] = maps.ignition.rpm_axis[0];
    assert(tunuino_tune_validate(&maps) == TUNUINO_TUNE_BAD_AXIS);
    assert(tunuino_tune_validate(0) == TUNUINO_TUNE_BAD_AXIS);
    return 0;
}
