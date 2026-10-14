// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TRIGGER_60_2_H
#define TUNUINO_TRIGGER_60_2_H
#include <stdint.h>
typedef struct {
    uint32_t previous_edge_us;
    uint32_t previous_normal_period_us;
    uint16_t tooth_index;
    uint8_t have_edge;
    uint8_t have_period;
    uint8_t synced;
    uint8_t normal_teeth_since_gap;
    uint32_t gaps_seen;
    uint32_t errors;
} tunuino_trigger_60_2_t;
void tunuino_trigger_60_2_init(tunuino_trigger_60_2_t *decoder);
// Returns 1 only when a missing-tooth gap is accepted.
// Caller supplies filtered rising-edge timestamps from a monotonic microsecond timer.
uint8_t tunuino_trigger_60_2_edge(tunuino_trigger_60_2_t *decoder, uint32_t timestamp_us);
void tunuino_trigger_60_2_lose_sync(tunuino_trigger_60_2_t *decoder);
#endif
