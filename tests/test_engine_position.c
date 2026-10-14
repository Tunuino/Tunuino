// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/engine_position.h"
#include <assert.h>
int main(void) {
    tunuino_engine_position_t p;
    tunuino_engine_position_init(&p);
    assert(!p.crank_reference_valid && !p.cycle_angle_valid);
    uint32_t t = 1000u;
    tunuino_engine_position_crank_edge(&p, t);
    for (unsigned i = 0; i < 8; ++i) {
        t += 1000u;
        tunuino_engine_position_crank_edge(&p, t);
    }
    t += 3000u;
    assert(tunuino_engine_position_crank_edge(&p, t));
    assert(p.crank_reference_valid);
    assert(p.crank_angle_deg == 0u);
    tunuino_engine_position_cam_edge(&p);
    assert(p.cycle_angle_valid);
    for (unsigned i = 0; i < 57; ++i) {
        t += 1000u;
        tunuino_engine_position_crank_edge(&p, t);
    }
    assert(p.crank_angle_deg == 342u);
    t += 3000u;
    assert(tunuino_engine_position_crank_edge(&p, t));
    assert(p.crank_angle_deg == 0u);
    assert(p.cycle_angle_deg == 360u);
    tunuino_engine_position_lose_sync(&p);
    assert(!p.crank_reference_valid && !p.cycle_angle_valid);
    return 0;
}
