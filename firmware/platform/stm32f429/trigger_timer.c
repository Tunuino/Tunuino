// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "trigger_timer.h"
#include <stddef.h>
void tunuino_trigger_timer_init(tunuino_trigger_timer_t *timer) {
    if (!timer) return;
    *timer = (tunuino_trigger_timer_t){0};
}
void tunuino_trigger_timer_ckp(tunuino_trigger_timer_t *timer, uint32_t timestamp_us) {
    if (!timer) return;
    timer->last_ckp_us = timestamp_us;
    timer->ckp_seen = 1u;
    if (timer->ckp_count != UINT32_MAX) ++timer->ckp_count;
}
void tunuino_trigger_timer_cmp(tunuino_trigger_timer_t *timer, uint32_t timestamp_us) {
    if (!timer) return;
    timer->last_cmp_us = timestamp_us;
    timer->cmp_seen = 1u;
    if (timer->cmp_count != UINT32_MAX) ++timer->cmp_count;
}
