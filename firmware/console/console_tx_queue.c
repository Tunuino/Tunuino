// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "console_tx_queue.h"
void tunuino_tx_queue_init(tunuino_tx_queue_t *q) {
    if (!q) return;
    *q = (tunuino_tx_queue_t){0};
}
size_t tunuino_tx_queue_write(const uint8_t *frame, size_t size, void *context) {
    tunuino_tx_queue_t *q = (tunuino_tx_queue_t *)context;
    if (!q || !frame || size == 0u || size > TUNUINO_TX_QUEUE_CAPACITY - q->count) {
        if (q && q->dropped_frames != UINT32_MAX) ++q->dropped_frames;
        return 0u;
    }
    for (size_t i = 0; i < size; ++i) {
        q->data[q->head] = frame[i];
        q->head = (q->head + 1u) % TUNUINO_TX_QUEUE_CAPACITY;
    }
    q->count += size;
    return size;
}
uint8_t tunuino_tx_queue_pop(tunuino_tx_queue_t *q, uint8_t *out) {
    if (!q || !out || !q->count) return 0u;
    *out = q->data[q->tail];
    q->tail = (q->tail + 1u) % TUNUINO_TX_QUEUE_CAPACITY;
    --q->count;
    return 1u;
}
size_t tunuino_tx_queue_size(const tunuino_tx_queue_t *q) {
    return q ? q->count : 0u;
}
