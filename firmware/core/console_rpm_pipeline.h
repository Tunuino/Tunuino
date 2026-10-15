// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_RPM_PIPELINE_H
#define TUNUINO_CONSOLE_RPM_PIPELINE_H
#include <stdint.h>
#include "engine_state.h"
#include "console_partial.h"
typedef struct {
    tunuino_engine_state_t engine;
    uint32_t max_rpm_age_us;
    uint8_t stopped_confirmed;
} tunuino_console_rpm_pipeline_t;
// Host-side CKP/CMP event processor, no hardware access.
void tunuino_console_rpm_pipeline_init(tunuino_console_rpm_pipeline_t *p,
    uint32_t min_tooth_us, uint32_t max_tooth_us,
    uint32_t crank_timeout_us, uint32_t max_rpm_age_us);
void tunuino_console_rpm_pipeline_ckp(tunuino_console_rpm_pipeline_t *p, uint32_t timestamp_us);
void tunuino_console_rpm_pipeline_cmp(tunuino_console_rpm_pipeline_t *p);
void tunuino_console_rpm_pipeline_confirm_stop(tunuino_console_rpm_pipeline_t *p, uint8_t confirmed);
void tunuino_console_rpm_pipeline_snapshot(tunuino_console_rpm_pipeline_t *p,
    uint32_t now_us, tunuino_console_partial_t *out);
#endif
