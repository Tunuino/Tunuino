// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_TUNE_RX_H
#define TUNUINO_CONSOLE_TUNE_RX_H
#include <stddef.h>
#include <stdint.h>
#include "console_tune_request.h"
// Byte-at-a-time, bounded read-only receive adapter. Single-threaded polling only.
typedef struct {
    uint8_t frame[11];
    uint8_t length;
    uint32_t accepted;
    uint32_t rejected;
} tunuino_console_tune_rx_t;
void tunuino_console_tune_rx_init(tunuino_console_tune_rx_t *rx);
void tunuino_console_tune_rx_byte(tunuino_console_tune_rx_t *rx,
    uint8_t byte, const tunuino_tune_wire_t *wire, tunuino_tx_queue_t *queue);
#endif
