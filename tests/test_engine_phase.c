// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/engine_phase.h"
#include <assert.h>
int main(void) {
    tunuino_engine_phase_t p;
    tunuino_engine_phase_init(&p);
    assert(p.state == TUNUINO_PHASE_UNSYNCED);
    tunuino_engine_phase_cam_event(&p);
    assert(p.state == TUNUINO_PHASE_UNSYNCED);
    tunuino_engine_phase_crank_reference(&p);
    assert(p.state == TUNUINO_PHASE_WAITING_CAM);
    tunuino_engine_phase_cam_event(&p);
    assert(p.state == TUNUINO_PHASE_SYNCED);
    tunuino_engine_phase_crank_reference(&p);
    assert(p.state == TUNUINO_PHASE_SYNCED);
    tunuino_engine_phase_crank_reference(&p);
    assert(p.state == TUNUINO_PHASE_SYNCED);
    tunuino_engine_phase_crank_reference(&p);
    tunuino_engine_phase_cam_event(&p);
    tunuino_engine_phase_crank_reference(&p);
    assert(p.state == TUNUINO_PHASE_SYNCED);
    tunuino_engine_phase_cam_event(&p);
    assert(p.state == TUNUINO_PHASE_SYNCED);
    tunuino_engine_phase_cam_event(&p);
    assert(p.state == TUNUINO_PHASE_UNSYNCED);
    assert(p.errors == 1u);
    tunuino_engine_phase_crank_reference(&p);
    assert(p.state == TUNUINO_PHASE_WAITING_CAM);
    tunuino_engine_phase_lose_sync(&p);
    assert(p.state == TUNUINO_PHASE_UNSYNCED);
    return 0;
}
