// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_rpm_pipeline.h"
#include <assert.h>
int main(void) {
    tunuino_console_rpm_pipeline_t p;
    tunuino_console_partial_t frame;
    tunuino_console_rpm_pipeline_init(&p, 100u, 10000u, 50000u, 20000u);
    tunuino_console_rpm_pipeline_snapshot(&p, 0u, &frame);
    assert(frame.valid_mask == 0u);
    // Explicitly confirmed stopped, including startup state.
    tunuino_console_rpm_pipeline_confirm_stop(&p, 1u);
    tunuino_console_rpm_pipeline_snapshot(&p, 1000u, &frame);
    assert(frame.valid_mask == TUNUINO_CONSOLE_FIELD_RPM && frame.fields.rpm == 0u);
    // Any new CKP activity revokes stopped confirmation.
    tunuino_console_rpm_pipeline_ckp(&p, 2000u);
    tunuino_console_rpm_pipeline_snapshot(&p, 2100u, &frame);
    assert(frame.valid_mask == 0u);
    // Exercise freshness and loss-of-signal on an already validated running state.
    p.engine.engine_running = 1u;
    p.engine.speed.valid = 1u;
    p.engine.speed.rpm = 1200u;
    p.engine.crank_edge_seen = 1u;
    p.engine.last_crank_edge_us = 5000u;
    tunuino_console_rpm_pipeline_snapshot(&p, 6000u, &frame);
    assert(frame.valid_mask == TUNUINO_CONSOLE_FIELD_RPM && frame.fields.rpm == 1200u);
    tunuino_console_rpm_pipeline_snapshot(&p, 26000u, &frame);
    assert(frame.valid_mask == 0u);
    tunuino_console_rpm_pipeline_snapshot(&p, 56000u, &frame);
    assert(frame.valid_mask == 0u && !p.engine.engine_running);
    // Recovery cannot be inferred from stale RPM; real CKP sync is required.
    tunuino_console_rpm_pipeline_ckp(&p, 57000u);
    tunuino_console_rpm_pipeline_snapshot(&p, 57100u, &frame);
    assert(frame.valid_mask == 0u);
    return 0;
}
