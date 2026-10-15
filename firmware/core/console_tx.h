// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_TX_H
#define TUNUINO_CONSOLE_TX_H
#include <stddef.h>
#include <stdint.h>
#include "console_protocol.h"
// Non-blocking transport callback: must return number of bytes accepted.
// A short write is treated as a failed frame; caller must supply atomic-frame transport.
typedef size_t (*tunuino_console_write_fn)(const uint8_t *frame, size_t size, void *context);
typedef struct {
    tunuino_console_write_fn write;
    void *context;
    uint32_t hello_frames;
    uint32_t telemetry_frames;
    uint32_t failed_frames;
} tunuino_console_tx_t;
void tunuino_console_tx_init(tunuino_console_tx_t *tx,
                             tunuino_console_write_fn write, void *context);
uint8_t tunuino_console_tx_hello(tunuino_console_tx_t *tx);
uint8_t tunuino_console_tx_telemetry(tunuino_console_tx_t *tx,
                                    const tunuino_console_telemetry_t *data);
#endif
