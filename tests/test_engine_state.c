// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/engine_state.h"
#include <assert.h>
int main(void) {
    tunuino_engine_state_t e;
    tunuino_engine_state_init(&e, 100u, 20000u, 30000u);
    assert(!e.engine_running);
    uint32_t t = 1000u;
    tunuino_engine_state_crank_edge(&e, t);
    for (unsigned i = 0; i < 8; ++i) {
        t += 1000u;
        tunuino_engine_state_crank_edge(&e, t);
    }
    t += 3000u;
    tunuino_engine_state_crank_edge(&e, t);
    assert(e.position.crank_reference_valid);
    tunuino_engine_state_cam_edge(&e);
    assert(e.position.cycle_angle_valid);
    for (unsigned i = 0; i < 3; ++i) {
        t += 1000u;
        tunuino_engine_state_crank_edge(&e, t);
    }
    assert(e.engine_running && e.speed.rpm == 1000u);
    tunuino_engine_state_poll(&e, t + 30001u);
    assert(!e.engine_running);
    assert(e.speed.rpm == 0u);
    assert(!e.position.crank_reference_valid);
    assert(!e.position.cycle_angle_valid);
    return 0;
}
