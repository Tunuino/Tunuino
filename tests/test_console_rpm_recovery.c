// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_rpm_pipeline.h"
#include <assert.h>
#include <stdint.h>
// Synthetic 60-2 wheel: 58 real teeth, 57 normal intervals, one 3-slot gap.
static uint32_t revolution(tunuino_console_rpm_pipeline_t *p, uint32_t t,
                           uint32_t tooth_us) {
    for (unsigned i = 0; i < 57u; ++i) {
        t += tooth_us;
        tunuino_console_rpm_pipeline_ckp(p, t);
    }
    t += tooth_us * 3u;
    tunuino_console_rpm_pipeline_ckp(p, t);
    return t;
}
int main(void) {
    tunuino_console_rpm_pipeline_t p;
    tunuino_console_partial_t out;
    tunuino_console_rpm_pipeline_init(&p, 100u, 10000u, 50000u, 10000u);
    uint32_t t = 1000u;
    tunuino_console_rpm_pipeline_ckp(&p, t);
    t = revolution(&p, t, 1000u);
    t = revolution(&p, t, 1000u);
    tunuino_console_rpm_pipeline_snapshot(&p, t, &out);
    assert(p.engine.position.crank.synced);
    assert(out.valid_mask == TUNUINO_CONSOLE_FIELD_RPM);
    assert(out.fields.rpm == 1000u);
    // No CKP edges: first stale RPM, then crank timeout and sync loss.
    tunuino_console_rpm_pipeline_snapshot(&p, t + 10001u, &out);
    assert(out.valid_mask == 0u);
    tunuino_console_rpm_pipeline_snapshot(&p, t + 50001u, &out);
    assert(out.valid_mask == 0u);
    assert(!p.engine.position.crank.synced);
    // Restarting wheel must reacquire gap and compute a new RPM.
    t += 60000u;
    tunuino_console_rpm_pipeline_ckp(&p, t);
    t = revolution(&p, t, 1000u);
    t = revolution(&p, t, 1000u);
    tunuino_console_rpm_pipeline_snapshot(&p, t, &out);
    assert(p.engine.position.crank.synced);
    assert(out.valid_mask == TUNUINO_CONSOLE_FIELD_RPM);
    assert(out.fields.rpm == 1000u);
    // Invalid extra-long interval must not publish stale RPM.
    tunuino_console_rpm_pipeline_ckp(&p, t + 10000u);
    tunuino_console_rpm_pipeline_snapshot(&p, t + 10000u, &out);
    assert(out.valid_mask == 0u);
    return 0;
}
