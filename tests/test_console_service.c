// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_service.h"
#include <assert.h>
typedef struct {uint8_t enabled; uint8_t bytes[256]; size_t count;} sink_t;
static uint8_t ready(void *ctx) {return ((sink_t *)ctx)->enabled;}
static void send(uint8_t byte, void *ctx) {
    sink_t *s = ctx;
    assert(s->count < sizeof(s->bytes));
    s->bytes[s->count++] = byte;
}
int main(void) {
    tunuino_console_service_t service;
    sink_t sink = {0u, {0}, 0u};
    tunuino_console_service_init(&service, ready, send, &sink);
    tunuino_console_service_step(&service, 0u, 0, 0u);
    assert(service.hello_queued && service.transmitter.hello_frames == 1u);
    assert(sink.count == 0u && tunuino_tx_queue_size(&service.queue) == 11u);
    const tunuino_console_telemetry_t sample = {1500u, 1013u, 325u, 1250u, 650u, 1470u, 1380u};
    tunuino_console_service_step(&service, 100u, &sample, 0u);
    assert(service.transmitter.telemetry_frames == 0u);
    tunuino_console_service_step(&service, 100u, &sample, 1u);
    assert(service.transmitter.telemetry_frames == 1u);
    tunuino_console_service_step(&service, 150u, &sample, 1u);
    assert(service.transmitter.telemetry_frames == 1u);
    sink.enabled = 1u;
    for (unsigned i = 0; i < 33u; ++i)
        tunuino_console_service_step(&service, 150u, 0, 0u);
    assert(sink.count == 33u && tunuino_tx_queue_size(&service.queue) == 0u);
    assert(sink.bytes[0] == 0x54u && sink.bytes[1] == 0x4eu);
    assert(sink.bytes[11] == 0x54u && sink.bytes[12] == 0x4eu);
    return 0;
}
