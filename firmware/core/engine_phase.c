// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "engine_phase.h"
#include <stddef.h>
void tunuino_engine_phase_init(tunuino_engine_phase_t *p) {
    if (!p) return;
    *p = (tunuino_engine_phase_t){0};
}
void tunuino_engine_phase_lose_sync(tunuino_engine_phase_t *p) {
    if (!p) return;
    p->state = TUNUINO_PHASE_UNSYNCED;
    p->revolution_parity = 0u;
    p->cam_seen_in_cycle = 0u;
}
void tunuino_engine_phase_crank_reference(tunuino_engine_phase_t *p) {
    if (!p) return;
    if (p->state == TUNUINO_PHASE_UNSYNCED) {
        p->state = TUNUINO_PHASE_WAITING_CAM;
        p->revolution_parity = 0u;
        p->cam_seen_in_cycle = 0u;
        p->revolutions = 1u;
        return;
    }
    if (p->revolutions != UINT32_MAX) ++p->revolutions;
    p->revolution_parity ^= 1u;
    if (p->state == TUNUINO_PHASE_SYNCED && p->revolution_parity == 0u) {
        if (p->cam_seen_in_cycle != 1u) {
            ++p->errors;
            tunuino_engine_phase_lose_sync(p);
            return;
        }
        p->cam_seen_in_cycle = 0u;
    }
}
void tunuino_engine_phase_cam_event(tunuino_engine_phase_t *p) {
    if (!p || p->state == TUNUINO_PHASE_UNSYNCED) return;
    if (p->cam_seen_in_cycle) {
        ++p->errors;
        tunuino_engine_phase_lose_sync(p);
        return;
    }
    p->cam_seen_in_cycle = 1u;
    if (p->state == TUNUINO_PHASE_WAITING_CAM) {
        // A cam pulse provides a candidate phase, not an angle-calibrated sync.
        p->state = TUNUINO_PHASE_SYNCED;
    }
}
