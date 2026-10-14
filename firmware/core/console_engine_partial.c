// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_engine_partial.h"
uint8_t tunuino_console_engine_partial(const tunuino_engine_state_t *engine,
    uint8_t stopped_confirmed, tunuino_console_partial_t *out) {
    if (!engine || !out) return 0u;
    tunuino_console_partial_t result = {0};
    if (engine->engine_running && engine->speed.valid &&
        engine->speed.rpm > 0u && engine->speed.rpm <= UINT16_MAX) {
        result.valid_mask = TUNUINO_CONSOLE_FIELD_RPM;
        result.fields.rpm = (uint16_t)engine->speed.rpm;
    } else if (!engine->engine_running && !engine->speed.valid && stopped_confirmed) {
        result.valid_mask = TUNUINO_CONSOLE_FIELD_RPM;
        result.fields.rpm = 0u;
    }
    *out = result;
    return 1u;
}
