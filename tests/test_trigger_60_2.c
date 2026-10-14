// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/trigger_60_2.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    tunuino_trigger_60_2_t d;
    tunuino_trigger_60_2_init(&d);
    uint32_t t = 1000u;
    tunuino_trigger_60_2_edge(&d, t);
    for (unsigned i = 0; i < 8; ++i) {
        t += 1000u;
        assert(!tunuino_trigger_60_2_edge(&d, t));
    }
    t += 3000u;
    assert(tunuino_trigger_60_2_edge(&d, t));
    assert(d.synced && d.tooth_index == 0u);
    for (unsigned i = 0; i < 57; ++i) {
        t += 1000u;
        assert(!tunuino_trigger_60_2_edge(&d, t));
    }
    assert(d.synced && d.tooth_index == 57u);
    t += 3000u;
    assert(tunuino_trigger_60_2_edge(&d, t));
    assert(d.gaps_seen == 2u);
    assert(d.errors == 0u);
    tunuino_trigger_60_2_lose_sync(&d);
    assert(!d.synced);
    tunuino_trigger_60_2_init(&d);
    assert(!d.synced && d.errors == 0u);
    return 0;
}
