// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_engine_partial.h"
#include "../firmware/core/console_partial_service.h"
#include <assert.h>
typedef struct {uint8_t bytes[64]; size_t size;} sink_t;
static uint8_t ready(void *ctx) {(void)ctx; return 1u;}
static void send(uint8_t byte, void *ctx) {
    sink_t *s = ctx;
    assert(s->size < sizeof(s->bytes));
    s->bytes[s->size++] = byte;
}
int main(void) {
    tunuino_engine_state_t engine = {0};
    tunuino_console_partial_t p;
    engine.engine_running = 1u;
    engine.speed.valid = 1u;
    engine.speed.rpm = 1800u;
    assert(tunuino_console_engine_partial(&engine, 0u, &p));
    assert(p.valid_mask == TUNUINO_CONSOLE_FIELD_RPM && p.fields.rpm == 1800u);
    engine.engine_running = 0u;
    engine.speed.valid = 0u;
    assert(tunuino_console_engine_partial(&engine, 0u, &p));
    assert(p.valid_mask == 0u);
    assert(tunuino_console_engine_partial(&engine, 1u, &p));
    assert(p.valid_mask == TUNUINO_CONSOLE_FIELD_RPM && p.fields.rpm == 0u);
    sink_t sink = {{0}, 0u};
    tunuino_console_service_t service;
    tunuino_console_service_init(&service, ready, send, &sink);
    tunuino_console_partial_service_step(&service, 100u, &p);
    assert(service.hello_queued);
    assert(service.last_telemetry_ms == 100u);
    assert(tunuino_tx_queue_size(&service.queue) == 34u);
    for (unsigned i = 0; i < 34u; ++i)
        tunuino_console_partial_service_step(&service, 100u, 0);
    assert(sink.size == 35u);
    assert(sink.bytes[0] == 0x54u && sink.bytes[11] == 0x54u);
    assert(sink.bytes[14] == 3u);
    return 0;
}
