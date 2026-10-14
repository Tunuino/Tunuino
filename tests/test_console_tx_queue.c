// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_tx_queue.h"
#include "../firmware/core/console_tx.h"
#include <assert.h>
int main(void) {
    tunuino_tx_queue_t q;
    tunuino_tx_queue_init(&q);
    tunuino_console_tx_t tx;
    tunuino_console_tx_init(&tx, tunuino_tx_queue_write, &q);
    assert(tunuino_console_tx_hello(&tx));
    assert(tunuino_tx_queue_size(&q) == 11u);
    const uint8_t expected[] = {0x54u, 0x4Eu, 1u, 1u, 3u, 0u, 0x54u, 0x4Eu, 1u};
    uint8_t byte = 0;
    for (unsigned i = 0; i < sizeof(expected); ++i) {
        assert(tunuino_tx_queue_pop(&q, &byte));
        assert(byte == expected[i]);
    }
    while (tunuino_tx_queue_pop(&q, &byte)) {}
    assert(tunuino_tx_queue_size(&q) == 0u);
    uint8_t big[250] = {0};
    assert(tunuino_tx_queue_write(big, sizeof(big), &q) == sizeof(big));
    const size_t before = tunuino_tx_queue_size(&q);
    const uint8_t frame[8] = {0};
    assert(tunuino_tx_queue_write(frame, sizeof(frame), &q) == 0u);
    assert(tunuino_tx_queue_size(&q) == before);
    assert(q.dropped_frames == 1u);
    for (unsigned i = 0; i < sizeof(big); ++i) assert(tunuino_tx_queue_pop(&q, &byte));
    assert(!tunuino_tx_queue_pop(&q, &byte));
    assert(tunuino_tx_queue_write(frame, sizeof(frame), &q) == sizeof(frame));
    return 0;
}
