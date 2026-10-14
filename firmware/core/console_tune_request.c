// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_tune_request.h"
#include "console_tx_queue.h"
uint8_t tunuino_console_tune_handle_frame(const tunuino_tune_wire_t *wire,
    const uint8_t *frame, size_t length, tunuino_tx_queue_t *queue) {
    if (!wire || !frame || !queue || length != 11u ||
        frame[0] != 0x54u || frame[1] != 0x4eu ||
        frame[2] != TUNUINO_CONSOLE_PROTOCOL_VERSION ||
        frame[3] != TUNUINO_CONSOLE_TYPE_TUNE_REQUEST ||
        frame[4] != 3u || frame[5] != 0u)
        return 0u;
    const uint16_t expected = (uint16_t)frame[9] | ((uint16_t)frame[10] << 8u);
    if (tunuino_console_crc16(frame + 2u, 7u) != expected) return 0u;
    uint8_t response[8u + 3u + TUNUINO_TUNE_WIRE_CHUNK_MAX];
    const size_t n = tunuino_console_tune_reply(wire, frame + 6u, 3u,
        response, sizeof(response));
    return n && tunuino_tx_queue_write(response, n, queue) == n;
}
