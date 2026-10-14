// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_rpm_pipeline.h"
#include "console_rpm_freshness.h"
void tunuino_console_rpm_pipeline_init(tunuino_console_rpm_pipeline_t *p,
    uint32_t min_tooth_us, uint32_t max_tooth_us,
    uint32_t crank_timeout_us, uint32_t max_rpm_age_us) {
    if (!p) return;
    *p = (tunuino_console_rpm_pipeline_t){0};
    tunuino_engine_state_init(&p->engine, min_tooth_us, max_tooth_us, crank_timeout_us);
    p->max_rpm_age_us = max_rpm_age_us;
}
void tunuino_console_rpm_pipeline_ckp(tunuino_console_rpm_pipeline_t *p, uint32_t timestamp_us) {
    if (!p) return;
    // New activity invalidates any prior independent stop confirmation.
    p->stopped_confirmed = 0u;
    tunuino_engine_state_crank_edge(&p->engine, timestamp_us);
}
void tunuino_console_rpm_pipeline_cmp(tunuino_console_rpm_pipeline_t *p) {
    if (!p) return;
    p->stopped_confirmed = 0u;
    tunuino_engine_state_cam_edge(&p->engine);
}
void tunuino_console_rpm_pipeline_confirm_stop(tunuino_console_rpm_pipeline_t *p, uint8_t confirmed) {
    if (!p) return;
    // Confirmation is caller-supplied, never inferred from CKP timeout.
    p->stopped_confirmed = (uint8_t)(confirmed != 0u);
}
void tunuino_console_rpm_pipeline_snapshot(tunuino_console_rpm_pipeline_t *p,
    uint32_t now_us, tunuino_console_partial_t *out) {
    if (!out) return;
    *out = (tunuino_console_partial_t){0};
    if (!p) return;
    tunuino_engine_state_poll(&p->engine, now_us);
    tunuino_console_rpm_fresh_snapshot(&p->engine, now_us,
        p->max_rpm_age_us, p->stopped_confirmed, out);
}
