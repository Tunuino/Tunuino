// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/engine_speed.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    tunuino_engine_speed_t s;
    tunuino_engine_speed_init(&s, 100u, 20000u, 30000u);
    tunuino_engine_speed_normal_tooth(&s, 1000u);
    assert(!s.valid);
    tunuino_engine_speed_normal_tooth(&s, 2000u);
    assert(s.valid && s.rpm == 1000u);
    tunuino_engine_speed_normal_tooth(&s, 2050u); // reject noise
    assert(s.rpm == 1000u);
    tunuino_engine_speed_gap(&s, 5000u);
    assert(s.rpm == 1000u);
    tunuino_engine_speed_normal_tooth(&s, 6000u);
    assert(s.valid && s.rpm == 1000u);
    tunuino_engine_speed_poll(&s, 40001u);
    assert(!s.valid && s.rpm == 0u);
    tunuino_engine_speed_init(&s, 100u, 20000u, 30000u);
    tunuino_engine_speed_normal_tooth(&s, UINT32_MAX - 499u);
    tunuino_engine_speed_normal_tooth(&s, 500u);
    assert(s.valid && s.rpm == 1000u);
    return 0;
}
