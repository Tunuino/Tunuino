// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_PARTIAL_SERVICE_H
#define TUNUINO_CONSOLE_PARTIAL_SERVICE_H
#include <stdint.h>
#include "console_service.h"
#include "console_partial.h"
// Uses the existing service queue and UART pump; 10 Hz limit.
// Must not be interleaved with console_service_step in the same tick.
void tunuino_console_partial_service_step(tunuino_console_service_t *service,
    uint32_t now_ms, const tunuino_console_partial_t *snapshot);
#endif
