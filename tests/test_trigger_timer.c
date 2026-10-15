// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/platform/stm32f429/trigger_timer.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    tunuino_trigger_timer_t timer;
    tunuino_trigger_timer_init(&timer);
    assert(!timer.ckp_seen && !timer.cmp_seen);
    tunuino_trigger_timer_ckp(&timer, 1000u);
    tunuino_trigger_timer_ckp(&timer, 2000u);
    tunuino_trigger_timer_cmp(&timer, 1500u);
    assert(timer.ckp_seen && timer.cmp_seen);
    assert(timer.ckp_count == 2u && timer.cmp_count == 1u);
    assert(timer.last_ckp_us == 2000u && timer.last_cmp_us == 1500u);
    return 0;
}
