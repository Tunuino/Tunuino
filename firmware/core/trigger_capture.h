// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TRIGGER_CAPTURE_H
#define TUNUINO_TRIGGER_CAPTURE_H
#include <stdint.h>
typedef enum {
    TUNUINO_TRIGGER_NO_SIGNAL = 0,
    TUNUINO_TRIGGER_ACQUIRING,
    TUNUINO_TRIGGER_VALID,
    TUNUINO_TRIGGER_LOST
} tunuino_trigger_state_t;
typedef struct {
    uint32_t last_edge_us;
    uint32_t last_period_us;
    uint32_t min_period_us;
    uint32_t max_period_us;
    uint16_t accepted_edges;
    uint8_t has_edge;
    tunuino_trigger_state_t state;
} tunuino_trigger_capture_t;
void tunuino_trigger_init(tunuino_trigger_capture_t *capture, uint32_t min_period_us, uint32_t max_period_us);
uint8_t tunuino_trigger_edge(tunuino_trigger_capture_t *capture, uint32_t timestamp_us);
void tunuino_trigger_check_timeout(tunuino_trigger_capture_t *capture, uint32_t now_us, uint32_t timeout_us);
#endif
