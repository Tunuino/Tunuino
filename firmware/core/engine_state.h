// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_ENGINE_STATE_H
#define TUNUINO_ENGINE_STATE_H
#include <stdint.h>
#include "engine_position.h"
#include "engine_speed.h"
typedef struct {
    tunuino_engine_position_t position;
    tunuino_engine_speed_t speed;
    uint32_t crank_timeout_us;
    uint32_t last_crank_edge_us;
    uint8_t crank_edge_seen;
    uint8_t engine_running;
} tunuino_engine_state_t;
void tunuino_engine_state_init(tunuino_engine_state_t *engine,
                               uint32_t min_tooth_us, uint32_t max_tooth_us,
                               uint32_t crank_timeout_us);
void tunuino_engine_state_crank_edge(tunuino_engine_state_t *engine, uint32_t timestamp_us);
void tunuino_engine_state_cam_edge(tunuino_engine_state_t *engine);
void tunuino_engine_state_poll(tunuino_engine_state_t *engine, uint32_t now_us);
#endif
