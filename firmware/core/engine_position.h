// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_ENGINE_POSITION_H
#define TUNUINO_ENGINE_POSITION_H
#include <stdint.h>
#include "trigger_60_2.h"
#include "engine_phase.h"
typedef struct {
    tunuino_trigger_60_2_t crank;
    tunuino_engine_phase_t phase;
    uint16_t crank_angle_deg;
    uint16_t cycle_angle_deg;
    uint8_t crank_reference_valid;
    uint8_t cycle_angle_valid;
} tunuino_engine_position_t;
void tunuino_engine_position_init(tunuino_engine_position_t *position);
// Returns 1 for an accepted crank reference gap.
uint8_t tunuino_engine_position_crank_edge(tunuino_engine_position_t *position, uint32_t timestamp_us);
// CMP must already be electrically conditioned and filtered.
void tunuino_engine_position_cam_edge(tunuino_engine_position_t *position);
void tunuino_engine_position_lose_sync(tunuino_engine_position_t *position);
#endif
