// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_RPM_FRESHNESS_H
#define TUNUINO_CONSOLE_RPM_FRESHNESS_H
#include <stdint.h>
#include "engine_state.h"
#include "console_partial.h"
// max_age_us must be nonzero and selected from the expected CKP timing.
uint8_t tunuino_console_rpm_fresh(const tunuino_engine_state_t *engine,
    uint32_t now_us, uint32_t max_age_us);
void tunuino_console_rpm_fresh_snapshot(const tunuino_engine_state_t *engine,
    uint32_t now_us, uint32_t max_age_us, uint8_t stopped_confirmed,
    tunuino_console_partial_t *out);
#endif
