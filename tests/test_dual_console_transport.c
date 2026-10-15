// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/dual_console_transport.h"
#include <assert.h>
#include <string.h>
typedef struct {uint8_t data[128]; size_t count;} sink_t;
static uint8_t ready(void *ctx) {(void)ctx; return 1u;}
static void send_byte(uint8_t byte, void *ctx) {
    sink_t *s = ctx;
    assert(s->count < sizeof(s->data));
    s->data[s->count++] = byte;
}
static size_t write_ts(const uint8_t *bytes, size_t n, void *ctx) {
    sink_t *s = ctx;
    if (n > sizeof(s->data) - s->count) return 0u;
    memcpy(s->data + s->count, bytes, n);
    s->count += n;
    return n;
}
int main(void) {
    tunuino_dual_transport_t dual;
    sink_t console = {{0},0u}, ts = {{0},0u};
    tunuino_dual_transport_init(&dual, ready, send_byte, &console, write_ts, &ts);
    assert(dual.configured);
    assert(tunuino_dual_transport_ts_receive(&dual, 'S', 0));
    assert(ts.count == strlen(TUNUINO_TS_SIGNATURE));
    assert(console.count == 0u);
    assert(!tunuino_dual_transport_ts_receive(&dual, 'W', 0));
    assert(ts.count == strlen(TUNUINO_TS_SIGNATURE));
    tunuino_dual_transport_console_step(&dual, 0u, 0, 0u);
    assert(console.count == 1u && console.data[0] == 0x54u);
    assert(ts.count == strlen(TUNUINO_TS_SIGNATURE));
    tunuino_dual_transport_init(&dual, ready, send_byte, &console, write_ts, &console);
    assert(!dual.configured);
    return 0;
}
