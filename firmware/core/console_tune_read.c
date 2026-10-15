// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "console_tune_read.h"
size_t tunuino_console_tune_reply(const tunuino_tune_wire_t *wire,
    const uint8_t *request, size_t request_size,
    uint8_t *frame, size_t capacity) {
    if (!wire || !wire->valid || !request || request_size != 3u || !frame)
        return 0u;
    const size_t offset = (size_t)request[0] | ((size_t)request[1] << 8u);
    const size_t count = request[2];
    if (!count || count > TUNUINO_TUNE_WIRE_CHUNK_MAX ||
        offset >= TUNUINO_TUNE_WIRE_SIZE ||
        count > TUNUINO_TUNE_WIRE_SIZE - offset)
        return 0u;
    uint8_t payload[3u + TUNUINO_TUNE_WIRE_CHUNK_MAX];
    payload[0] = request[0];
    payload[1] = request[1];
    payload[2] = request[2];
    if (tunuino_tune_wire_read(wire, offset, payload + 3u, count) != count)
        return 0u;
    return tunuino_console_encode(TUNUINO_CONSOLE_TYPE_TUNE_CHUNK,
        payload, count + 3u, frame, capacity);
}
