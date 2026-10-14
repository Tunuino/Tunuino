// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "trigger_60_2.h"
#include <stddef.h>
#define NORMAL_INTERVALS_BETWEEN_GAPS 57u
void tunuino_trigger_60_2_init(tunuino_trigger_60_2_t *d) {
    if (!d) return;
    *d = (tunuino_trigger_60_2_t){0};
}
void tunuino_trigger_60_2_lose_sync(tunuino_trigger_60_2_t *d) {
    if (!d) return;
    d->synced = 0u;
    d->have_period = 0u;
    d->normal_teeth_since_gap = 0u;
    d->tooth_index = 0u;
}
uint8_t tunuino_trigger_60_2_edge(tunuino_trigger_60_2_t *d, uint32_t t) {
    if (!d) return 0u;
    if (!d->have_edge) {
        d->previous_edge_us = t;
        d->have_edge = 1u;
        return 0u;
    }
    const uint32_t dt = t - d->previous_edge_us;
    if (dt == 0u) return 0u;
    d->previous_edge_us = t;
    if (!d->have_period) {
        d->previous_normal_period_us = dt;
        d->have_period = 1u;
        return 0u;
    }
    const uint32_t baseline = d->previous_normal_period_us;
    const uint64_t scaled = (uint64_t)dt * 2u;
    const uint64_t normal = (uint64_t)baseline;
    const uint8_t gap = (uint8_t)(scaled >= normal * 5u && scaled <= normal * 7u);
    if (gap) {
        if (d->synced && d->normal_teeth_since_gap != NORMAL_INTERVALS_BETWEEN_GAPS) {
            ++d->errors;
            tunuino_trigger_60_2_lose_sync(d);
        }
        d->synced = 1u;
        d->tooth_index = 0u;
        d->normal_teeth_since_gap = 0u;
        ++d->gaps_seen;
        // Preserve the normal tooth period so the first post-gap tooth is counted.
        d->previous_normal_period_us = baseline;
        d->have_period = 1u;
        return 1u;
    }
    if (scaled > normal * 4u || scaled < normal) {
        ++d->errors;
        tunuino_trigger_60_2_lose_sync(d);
        return 0u;
    }
    d->previous_normal_period_us = dt;
    if (d->synced) {
        if (d->normal_teeth_since_gap >= NORMAL_INTERVALS_BETWEEN_GAPS) {
            ++d->errors;
            tunuino_trigger_60_2_lose_sync(d);
            return 0u;
        }
        ++d->normal_teeth_since_gap;
        d->tooth_index = d->normal_teeth_since_gap;
    }
    return 0u;
}
