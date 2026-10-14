// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/tunerstudio_serial.h"
#include <assert.h>
#include <string.h>
typedef struct {
    uint8_t bytes[64];
    size_t size;
    uint8_t fail;
} sink_t;
static size_t write_response(const uint8_t *bytes, size_t count, void *context) {
    sink_t *sink = context;
    if (sink->fail || count > sizeof(sink->bytes) - sink->size) return 0u;
    memcpy(sink->bytes + sink->size, bytes, count);
    sink->size += count;
    return count;
}
int main(void) {
    sink_t sink = {{0}, 0u, 0u};
    tunuino_ts_serial_t serial;
    tunuino_ts_serial_init(&serial, write_response, &sink);
    assert(tunuino_ts_serial_receive(&serial, 'S', 0));
    assert(sink.size == strlen(TUNUINO_TS_SIGNATURE));
    assert(memcmp(sink.bytes, TUNUINO_TS_SIGNATURE, sink.size) == 0);
    sink.size = 0u;
    const tunuino_ts_values_t values = {1500u, 1000u, 1270u, 1u, 1u, 1u};
    assert(tunuino_ts_serial_receive(&serial, 'A', &values));
    assert(sink.size == 8u && sink.bytes[0] == 0xdcu && sink.bytes[1] == 0x05u);
    assert(!tunuino_ts_serial_receive(&serial, 'W', &values));
    assert(serial.commands_rejected == 1u && serial.commands_handled == 2u);
    sink.fail = 1u;
    assert(!tunuino_ts_serial_receive(&serial, 'S', 0));
    assert(serial.responses_failed == 1u);
    return 0;
}
