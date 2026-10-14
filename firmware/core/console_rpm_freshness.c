// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_rpm_freshness.h"
uint8_t tunuino_console_rpm_fresh(const tunuino_engine_state_t *engine,
    uint32_t now_us, uint32_t max_age_us) {
    if (!engine || !max_age_us || !engine->crank_edge_seen ||
        !engine->engine_running || !engine->speed.valid ||
        engine->speed.rpm == 0u || engine->speed.rpm > UINT16_MAX)
        return 0u;
    // Unsigned subtraction also handles 32-bit microsecond counter wraparound.
    return (uint8_t)((uint32_t)(now_us - engine->last_crank_edge_us) <= max_age_us);
}
void tunuino_console_rpm_fresh_snapshot(const tunuino_engine_state_t *engine,
    uint32_t now_us, uint32_t max_age_us, uint8_t stopped_confirmed,
    tunuino_console_partial_t *out) {
    if (!out) return;
    *out = (tunuino_console_partial_t){0};
    if (!engine) return;
    if (tunuino_console_rpm_fresh(engine, now_us, max_age_us)) {
        out->valid_mask = TUNUINO_CONSOLE_FIELD_RPM;
        out->fields.rpm = (uint16_t)engine->speed.rpm;
    } else if (stopped_confirmed && !engine->engine_running &&
               !engine->speed.valid) {
        out->valid_mask = TUNUINO_CONSOLE_FIELD_RPM;
        out->fields.rpm = 0u;
    }
}
