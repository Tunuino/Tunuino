// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_tx.h"
#include <stddef.h>
static uint8_t send_frame(tunuino_console_tx_t *tx, const uint8_t *frame,
                          size_t size, uint8_t is_hello) {
    if (!tx || !tx->write || !size) return 0u;
    if (tx->write(frame, size, tx->context) != size) {
        if (tx->failed_frames != UINT32_MAX) ++tx->failed_frames;
        return 0u;
    }
    uint32_t *counter = is_hello ? &tx->hello_frames : &tx->telemetry_frames;
    if (*counter != UINT32_MAX) ++*counter;
    return 1u;
}
void tunuino_console_tx_init(tunuino_console_tx_t *tx,
                             tunuino_console_write_fn write, void *context) {
    if (!tx) return;
    *tx = (tunuino_console_tx_t){0};
    tx->write = write;
    tx->context = context;
}
uint8_t tunuino_console_tx_hello(tunuino_console_tx_t *tx) {
    const uint8_t identity[] = {0x54u, 0x4Eu, TUNUINO_CONSOLE_PROTOCOL_VERSION};
    uint8_t frame[sizeof(identity) + TUNUINO_CONSOLE_FRAME_OVERHEAD];
    const size_t size = tunuino_console_encode(TUNUINO_CONSOLE_TYPE_HELLO,
        identity, sizeof(identity), frame, sizeof(frame));
    return send_frame(tx, frame, size, 1u);
}
uint8_t tunuino_console_tx_telemetry(tunuino_console_tx_t *tx,
                                    const tunuino_console_telemetry_t *data) {
    if (!data) return 0u;
    uint8_t frame[14u + TUNUINO_CONSOLE_FRAME_OVERHEAD];
    const size_t size = tunuino_console_encode_telemetry(data, frame, sizeof(frame));
    return send_frame(tx, frame, size, 0u);
}
