// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/trigger_capture.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    tunuino_trigger_capture_t c;
    tunuino_trigger_init(&c, 100u, 10000u);
    assert(c.state == TUNUINO_TRIGGER_NO_SIGNAL);
    assert(!tunuino_trigger_edge(&c, 1000u));
    assert(!tunuino_trigger_edge(&c, 1050u)); // glitch
    assert(tunuino_trigger_edge(&c, 2000u));
    assert(c.last_period_us == 1000u);
    assert(tunuino_trigger_edge(&c, 3000u));
    assert(c.state == TUNUINO_TRIGGER_ACQUIRING);
    assert(tunuino_trigger_edge(&c, 4000u));
    assert(c.state == TUNUINO_TRIGGER_VALID);
    tunuino_trigger_check_timeout(&c, 15000u, 5000u);
    assert(c.state == TUNUINO_TRIGGER_LOST);
    assert(c.last_period_us == 0u);
    assert(!tunuino_trigger_edge(&c, 16000u));
    assert(c.state == TUNUINO_TRIGGER_ACQUIRING);
    assert(!tunuino_trigger_edge(&c, 30000u)); // gap too long
    assert(c.last_period_us == 0u);
    tunuino_trigger_init(&c, 100u, 10000u);
    assert(!tunuino_trigger_edge(&c, UINT32_MAX - 500u));
    assert(tunuino_trigger_edge(&c, 499u)); // timer rollover
    assert(c.last_period_us == 1000u);
    return 0;
}
