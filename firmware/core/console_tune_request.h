// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_TUNE_REQUEST_H
#define TUNUINO_CONSOLE_TUNE_REQUEST_H
#include <stddef.h>
#include <stdint.h>
#include "console_tune_read.h"
#define TUNUINO_CONSOLE_TYPE_TUNE_REQUEST 5u
// Handles one fully validated Console v1 frame. Read-only.
// Queues a complete type-4 response atomically, or rejects the request.
uint8_t tunuino_console_tune_handle_frame(const tunuino_tune_wire_t *wire,
    const uint8_t *frame, size_t length, tunuino_tx_queue_t *queue);
#endif
