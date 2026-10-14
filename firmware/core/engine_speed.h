// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_ENGINE_SPEED_H
#define TUNUINO_ENGINE_SPEED_H
#include <stdint.h>
typedef struct {
    uint32_t last_edge_us;
    uint32_t last_period_us;
    uint32_t rpm;
    uint32_t min_period_us;
    uint32_t max_period_us;
    uint32_t timeout_us;
    uint8_t have_edge;
    uint8_t valid;
} tunuino_engine_speed_t;
// 60-2 has 60 nominal tooth slots per crank revolution.
void tunuino_engine_speed_init(tunuino_engine_speed_t *speed, uint32_t min_period_us,
                               uint32_t max_period_us, uint32_t timeout_us);
void tunuino_engine_speed_normal_tooth(tunuino_engine_speed_t *speed, uint32_t timestamp_us);
void tunuino_engine_speed_gap(tunuino_engine_speed_t *speed, uint32_t timestamp_us);
void tunuino_engine_speed_poll(tunuino_engine_speed_t *speed, uint32_t now_us);
#endif
