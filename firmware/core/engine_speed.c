// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "engine_speed.h"
#include <stddef.h>
#define TUNUINO_RPM_NUMERATOR 1000000u
void tunuino_engine_speed_init(tunuino_engine_speed_t *s, uint32_t min_us,
                               uint32_t max_us, uint32_t timeout_us) {
    if (!s) return;
    *s = (tunuino_engine_speed_t){0};
    s->min_period_us = min_us;
    s->max_period_us = max_us;
    s->timeout_us = timeout_us;
}
void tunuino_engine_speed_normal_tooth(tunuino_engine_speed_t *s, uint32_t t) {
    if (!s) return;
    if (!s->have_edge) {
        s->last_edge_us = t;
        s->have_edge = 1u;
        return;
    }
    const uint32_t dt = t - s->last_edge_us;
    if (dt < s->min_period_us || s->min_period_us == 0u) return;
    s->last_edge_us = t;
    if (dt > s->max_period_us) {
        s->valid = 0u;
        s->rpm = 0u;
        s->last_period_us = 0u;
        return;
    }
    s->last_period_us = dt;
    // 60 tooth slots/rev: RPM = 60,000,000 / (60 * period_us).
    s->rpm = TUNUINO_RPM_NUMERATOR / dt;
    s->valid = 1u;
}
void tunuino_engine_speed_gap(tunuino_engine_speed_t *s, uint32_t t) {
    if (!s) return;
    // A missing-tooth gap is not a normal tooth interval.
    // Reset the edge reference, retaining the last valid RPM estimate.
    s->last_edge_us = t;
    s->have_edge = 1u;
}
void tunuino_engine_speed_poll(tunuino_engine_speed_t *s, uint32_t now_us) {
    if (!s || !s->have_edge || !s->timeout_us) return;
    if ((uint32_t)(now_us - s->last_edge_us) > s->timeout_us) {
        s->valid = 0u;
        s->rpm = 0u;
        s->last_period_us = 0u;
        s->have_edge = 0u;
    }
}
