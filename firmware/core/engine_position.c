// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "engine_position.h"
#include <stddef.h>
void tunuino_engine_position_init(tunuino_engine_position_t *p) {
    if (!p) return;
    tunuino_trigger_60_2_init(&p->crank);
    tunuino_engine_phase_init(&p->phase);
    p->crank_angle_deg = 0u;
    p->cycle_angle_deg = 0u;
    p->crank_reference_valid = 0u;
    p->cycle_angle_valid = 0u;
}
void tunuino_engine_position_lose_sync(tunuino_engine_position_t *p) {
    if (!p) return;
    tunuino_trigger_60_2_lose_sync(&p->crank);
    tunuino_engine_phase_lose_sync(&p->phase);
    p->crank_reference_valid = 0u;
    p->cycle_angle_valid = 0u;
    p->crank_angle_deg = 0u;
    p->cycle_angle_deg = 0u;
}
uint8_t tunuino_engine_position_crank_edge(tunuino_engine_position_t *p, uint32_t timestamp_us) {
    if (!p) return 0u;
    const uint32_t old_errors = p->crank.errors;
    const uint8_t gap = tunuino_trigger_60_2_edge(&p->crank, timestamp_us);
    if (p->crank.errors != old_errors || !p->crank.synced) {
        tunuino_engine_phase_lose_sync(&p->phase);
        p->crank_reference_valid = 0u;
        p->cycle_angle_valid = 0u;
        return 0u;
    }
    if (gap) {
        tunuino_engine_phase_crank_reference(&p->phase);
    }
    p->crank_reference_valid = 1u;
    p->crank_angle_deg = (uint16_t)(p->crank.tooth_index * 6u);
    // The 720-degree angle is only a relative candidate until cam-tooth
    // window and cylinder-TDC calibration have been verified.
    p->cycle_angle_valid = (uint8_t)(p->phase.state == TUNUINO_PHASE_SYNCED);
    if (p->cycle_angle_valid) {
        p->cycle_angle_deg = (uint16_t)(p->crank_angle_deg +
                                         (p->phase.revolution_parity ? 360u : 0u));
    } else {
        p->cycle_angle_deg = 0u;
    }
    return gap;
}
void tunuino_engine_position_cam_edge(tunuino_engine_position_t *p) {
    if (!p || !p->crank_reference_valid) return;
    tunuino_engine_phase_cam_event(&p->phase);
    p->cycle_angle_valid = (uint8_t)(p->phase.state == TUNUINO_PHASE_SYNCED);
    if (p->cycle_angle_valid) {
        p->cycle_angle_deg = (uint16_t)(p->crank_angle_deg +
                                         (p->phase.revolution_parity ? 360u : 0u));
    } else {
        p->cycle_angle_deg = 0u;
    }
}
