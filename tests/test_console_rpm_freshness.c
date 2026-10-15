// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_rpm_freshness.h"
#include <assert.h>
int main(void) {
    tunuino_engine_state_t engine = {0};
    engine.engine_running = 1u;
    engine.speed.valid = 1u;
    engine.speed.rpm = 2000u;
    engine.crank_edge_seen = 1u;
    engine.last_crank_edge_us = 100000u;
    tunuino_console_partial_t snapshot;
    tunuino_console_rpm_fresh_snapshot(&engine, 110000u, 20000u, 0u, &snapshot);
    assert(snapshot.valid_mask == 1u && snapshot.fields.rpm == 2000u);
    tunuino_console_rpm_fresh_snapshot(&engine, 120001u, 20000u, 0u, &snapshot);
    assert(snapshot.valid_mask == 0u);
    engine.last_crank_edge_us = 0xfffffff0u;
    assert(tunuino_console_rpm_fresh(&engine, 0x10u, 100u));
    engine.crank_edge_seen = 0u;
    assert(!tunuino_console_rpm_fresh(&engine, 0x10u, 100u));
    engine.engine_running = 0u;
    engine.speed.valid = 0u;
    tunuino_console_rpm_fresh_snapshot(&engine, 0u, 20000u, 1u, &snapshot);
    assert(snapshot.valid_mask == 1u && snapshot.fields.rpm == 0u);
    return 0;
}
