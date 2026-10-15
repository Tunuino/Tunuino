// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_ENGINE_PARTIAL_H
#define TUNUINO_CONSOLE_ENGINE_PARTIAL_H
#include <stdint.h>
#include "engine_state.h"
#include "console_partial.h"
// RPM-only telemetry. Other fields always marked unavailable.
// stopped_confirmed must be set only by a separately verified engine-stop detector.
uint8_t tunuino_console_engine_partial(const tunuino_engine_state_t *engine,
    uint8_t stopped_confirmed, tunuino_console_partial_t *out);
#endif
