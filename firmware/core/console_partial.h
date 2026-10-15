// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_PARTIAL_H
#define TUNUINO_CONSOLE_PARTIAL_H
#include <stddef.h>
#include <stdint.h>
#include "console_protocol.h"
#define TUNUINO_CONSOLE_TYPE_PARTIAL 3u
#define TUNUINO_CONSOLE_FIELD_RPM (1u << 0)
#define TUNUINO_CONSOLE_FIELD_MAP (1u << 1)
#define TUNUINO_CONSOLE_FIELD_TPS (1u << 2)
#define TUNUINO_CONSOLE_FIELD_CLT (1u << 3)
#define TUNUINO_CONSOLE_FIELD_IAT (1u << 4)
#define TUNUINO_CONSOLE_FIELD_AFR (1u << 5)
#define TUNUINO_CONSOLE_FIELD_BATTERY (1u << 6)
typedef struct {
    uint16_t valid_mask;
    tunuino_console_telemetry_t fields;
} tunuino_console_partial_t;
size_t tunuino_console_encode_partial(const tunuino_console_partial_t *data,
                                      uint8_t *output, size_t capacity);
#endif
