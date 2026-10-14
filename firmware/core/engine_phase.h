// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_ENGINE_PHASE_H
#define TUNUINO_ENGINE_PHASE_H
#include <stdint.h>
typedef enum {
    TUNUINO_PHASE_UNSYNCED = 0,
    TUNUINO_PHASE_WAITING_CAM,
    TUNUINO_PHASE_SYNCED
} tunuino_phase_state_t;
typedef struct {
    tunuino_phase_state_t state;
    uint8_t revolution_parity;
    uint8_t cam_seen_in_cycle;
    uint32_t revolutions;
    uint32_t errors;
} tunuino_engine_phase_t;
void tunuino_engine_phase_init(tunuino_engine_phase_t *phase);
// Call only on a verified crank reference gap.
void tunuino_engine_phase_crank_reference(tunuino_engine_phase_t *phase);
// Call for an already filtered and electrically conditioned cam event.
// Initial prototype assumes exactly one cam pulse per 720 crank degrees.
void tunuino_engine_phase_cam_event(tunuino_engine_phase_t *phase);
// Call on crank signal loss, implausible tooth pattern or timeout.
void tunuino_engine_phase_lose_sync(tunuino_engine_phase_t *phase);
#endif
