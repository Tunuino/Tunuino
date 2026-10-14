// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_tx.h"
#include <assert.h>
#include <string.h>
typedef struct {uint8_t data[32]; size_t size; uint8_t short_write;} sink_t;
static size_t write_frame(const uint8_t *frame, size_t size, void *ctx) {
    sink_t *sink = ctx;
    assert(size <= sizeof(sink->data));
    memcpy(sink->data, frame, size);
    sink->size = size;
    return sink->short_write ? size - 1u : size;
}
int main(void) {
    sink_t sink = {{0}, 0u, 0u};
    tunuino_console_tx_t tx;
    tunuino_console_tx_init(&tx, write_frame, &sink);
    assert(tunuino_console_tx_hello(&tx));
    assert(sink.size == 11u && sink.data[3] == TUNUINO_CONSOLE_TYPE_HELLO);
    assert(sink.data[6] == 0x54u && sink.data[7] == 0x4Eu && sink.data[8] == 1u);
    const tunuino_console_telemetry_t data = {1500u, 1000u, 200u, 1200u, 650u, 1470u, 1380u};
    assert(tunuino_console_tx_telemetry(&tx, &data));
    assert(sink.size == 22u && sink.data[3] == TUNUINO_CONSOLE_TYPE_TELEMETRY);
    assert(tx.hello_frames == 1u && tx.telemetry_frames == 1u);
    sink.short_write = 1u;
    assert(!tunuino_console_tx_hello(&tx));
    assert(tx.failed_frames == 1u);
    assert(!tunuino_console_tx_telemetry(&tx, 0));
    return 0;
}
