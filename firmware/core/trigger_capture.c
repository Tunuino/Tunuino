// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "trigger_capture.h"
void tunuino_trigger_init(tunuino_trigger_capture_t *capture, uint32_t min_period_us, uint32_t max_period_us) {
    if (!capture) return;
    capture->last_edge_us = 0u;
    capture->last_period_us = 0u;
    capture->min_period_us = min_period_us;
    capture->max_period_us = max_period_us;
    capture->accepted_edges = 0u;
    capture->has_edge = 0u;
    capture->state = TUNUINO_TRIGGER_NO_SIGNAL;
}
uint8_t tunuino_trigger_edge(tunuino_trigger_capture_t *capture, uint32_t timestamp_us) {
    if (!capture || capture->min_period_us == 0u ||
        capture->min_period_us > capture->max_period_us) return 0u;
    if (!capture->has_edge) {
        capture->last_edge_us = timestamp_us;
        capture->has_edge = 1u;
        capture->accepted_edges = 0u;
        capture->state = TUNUINO_TRIGGER_ACQUIRING;
        return 0u;
    }
    const uint32_t delta = timestamp_us - capture->last_edge_us;
    if (delta < capture->min_period_us) return 0u; // Reject fast glitches without moving reference.
    capture->last_edge_us = timestamp_us;
    if (delta > capture->max_period_us) {
        capture->accepted_edges = 0u;
        capture->last_period_us = 0u;
        capture->state = TUNUINO_TRIGGER_ACQUIRING;
        return 0u;
    }
    capture->last_period_us = delta;
    if (capture->accepted_edges < UINT16_MAX) ++capture->accepted_edges;
    capture->state = capture->accepted_edges >= 3u ? TUNUINO_TRIGGER_VALID : TUNUINO_TRIGGER_ACQUIRING;
    return 1u;
}
void tunuino_trigger_check_timeout(tunuino_trigger_capture_t *capture, uint32_t now_us, uint32_t timeout_us) {
    if (!capture || !capture->has_edge || timeout_us == 0u) return;
    if ((uint32_t)(now_us - capture->last_edge_us) > timeout_us) {
        capture->state = TUNUINO_TRIGGER_LOST;
        capture->accepted_edges = 0u;
        capture->last_period_us = 0u;
        capture->has_edge = 0u;
    }
}
