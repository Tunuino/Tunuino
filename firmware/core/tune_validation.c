// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "tune_validation.h"
tunuino_tune_error_t tunuino_tune_validate(const tunuino_tune_maps_t *maps) {
    if (!maps || !tunuino_tune_map_valid(&maps->ve) ||
        !tunuino_tune_map_valid(&maps->ignition))
        return TUNUINO_TUNE_BAD_AXIS;
    const tunuino_tune_map_t *tables[2] = {&maps->ve, &maps->ignition};
    for (unsigned t = 0u; t < 2u; ++t) {
        const tunuino_tune_map_t *m = tables[t];
        for (unsigned i = 0u; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
            if (m->rpm_axis[i] < 100u || m->rpm_axis[i] > 16000u)
                return TUNUINO_TUNE_BAD_RPM;
            if (m->load_axis[i] < 100u || m->load_axis[i] > 4000u)
                return TUNUINO_TUNE_BAD_LOAD;
            for (unsigned j = 0u; j < TUNUINO_MAP_AXIS_SIZE; ++j) {
                const int16_t v = m->cells[i][j];
                if (t == 0u && (v < 0 || v > 3000))
                    return TUNUINO_TUNE_BAD_VE;
                if (t == 1u && (v < -400 || v > 800))
                    return TUNUINO_TUNE_BAD_IGNITION;
            }
        }
    }
    return TUNUINO_TUNE_OK;
}
