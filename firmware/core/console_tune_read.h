// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_TUNE_READ_H
#define TUNUINO_CONSOLE_TUNE_READ_H
#include <stddef.h>
#include <stdint.h>
#include "console_protocol.h"
#include "tune_wire.h"
#define TUNUINO_CONSOLE_TYPE_TUNE_CHUNK 4u
// Request payload: offset u16 LE, count u8 (1..32).
// Response payload: offset u16 LE, count u8, bytes[count].
size_t tunuino_console_tune_reply(const tunuino_tune_wire_t *wire,
    const uint8_t *request, size_t request_size,
    uint8_t *frame, size_t capacity);
#endif
