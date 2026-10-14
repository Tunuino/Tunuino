// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_TX_QUEUE_H
#define TUNUINO_CONSOLE_TX_QUEUE_H
#include <stddef.h>
#include <stdint.h>
#define TUNUINO_TX_QUEUE_CAPACITY 256u
typedef struct {
    uint8_t data[TUNUINO_TX_QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    uint32_t dropped_frames;
} tunuino_tx_queue_t;
void tunuino_tx_queue_init(tunuino_tx_queue_t *queue);
// Atomic frame insertion: if frame does not fit, no bytes are queued.
size_t tunuino_tx_queue_write(const uint8_t *frame, size_t size, void *context);
// Returns 1 if a byte was removed, 0 if empty. Intended for UART TX-ready handler.
uint8_t tunuino_tx_queue_pop(tunuino_tx_queue_t *queue, uint8_t *out);
size_t tunuino_tx_queue_size(const tunuino_tx_queue_t *queue);
#endif
