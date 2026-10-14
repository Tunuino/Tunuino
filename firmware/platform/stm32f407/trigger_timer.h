// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TRIGGER_TIMER_H
#define TUNUINO_TRIGGER_TIMER_H
#include <stdint.h>
// Logical timestamp adapter. A hardware capture driver must provide 1-us timestamps.
// This API deliberately does not configure GPIO or enable timer interrupts.
typedef struct {
    uint32_t last_ckp_us;
    uint32_t last_cmp_us;
    uint32_t ckp_count;
    uint32_t cmp_count;
    uint8_t ckp_seen;
    uint8_t cmp_seen;
} tunuino_trigger_timer_t;
void tunuino_trigger_timer_init(tunuino_trigger_timer_t *timer);
void tunuino_trigger_timer_ckp(tunuino_trigger_timer_t *timer, uint32_t timestamp_us);
void tunuino_trigger_timer_cmp(tunuino_trigger_timer_t *timer, uint32_t timestamp_us);
#endif
