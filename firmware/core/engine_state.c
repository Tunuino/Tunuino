// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "engine_state.h"
#include <stddef.h>
void tunuino_engine_state_init(tunuino_engine_state_t *e, uint32_t min_us,
                               uint32_t max_us, uint32_t timeout_us) {
    if (!e) return;
    tunuino_engine_position_init(&e->position);
    tunuino_engine_speed_init(&e->speed, min_us, max_us, timeout_us);
    e->crank_timeout_us = timeout_us;
    e->last_crank_edge_us = 0u;
    e->crank_edge_seen = 0u;
    e->engine_running = 0u;
}
void tunuino_engine_state_crank_edge(tunuino_engine_state_t *e, uint32_t t) {
    if (!e) return;
    // First timestamp the raw edge for stall detection. This does not imply sync.
    e->last_crank_edge_us = t;
    e->crank_edge_seen = 1u;
    const uint8_t was_synced = e->position.crank.synced;
    const uint16_t previous_tooth = e->position.crank.tooth_index;
    const uint32_t old_errors = e->position.crank.errors;
    const uint8_t gap = tunuino_engine_position_crank_edge(&e->position, t);
    if (e->position.crank.errors != old_errors) {
        e->speed.valid = 0u;
        e->speed.rpm = 0u;
        e->engine_running = 0u;
        return;
    }
    if (gap) {
        tunuino_engine_speed_gap(&e->speed, t);
    } else if (e->position.crank.synced && was_synced &&
               e->position.crank.tooth_index != previous_tooth) {
        tunuino_engine_speed_normal_tooth(&e->speed, t);
    }
    e->engine_running = (uint8_t)(e->position.crank_reference_valid && e->speed.valid);
}
void tunuino_engine_state_cam_edge(tunuino_engine_state_t *e) {
    if (!e) return;
    tunuino_engine_position_cam_edge(&e->position);
}
void tunuino_engine_state_poll(tunuino_engine_state_t *e, uint32_t now_us) {
    if (!e) return;
    tunuino_engine_speed_poll(&e->speed, now_us);
    if (e->crank_edge_seen && e->crank_timeout_us &&
        (uint32_t)(now_us - e->last_crank_edge_us) > e->crank_timeout_us) {
        tunuino_engine_position_lose_sync(&e->position);
        e->speed.valid = 0u;
        e->speed.rpm = 0u;
        e->engine_running = 0u;
        e->crank_edge_seen = 0u;
    }
    if (!e->speed.valid) e->engine_running = 0u;
}
