// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/dual_console_transport_v2.h"
#include <assert.h>
#include <string.h>
typedef struct {uint8_t bytes[128]; size_t n;} sink_t;
static uint8_t ready(void *ctx) {(void)ctx; return 1u;}
static void send(uint8_t byte, void *ctx) {
    sink_t *s = ctx;
    assert(s->n < sizeof(s->bytes));
    s->bytes[s->n++] = byte;
}
int main(void) {
    sink_t a = {{0}, 0u}, b = {{0}, 0u};
    tunuino_dual_transport_v2_t d;
    tunuino_dual_transport_v2_init(&d, ready, send, &a, ready, send, &b);
    assert(d.configured);
    tunuino_console_partial_t partial = {0};
    partial.valid_mask = TUNUINO_CONSOLE_FIELD_RPM;
    partial.fields.rpm = 1500u;
    assert(tunuino_dual_transport_v2_ts_receive(&d, 'S', 0));
    tunuino_dual_transport_v2_step(&d, 100u, &partial);
    assert(a.n == 1u && a.bytes[0] == 0x54u);
    assert(b.n == 1u && b.bytes[0] == 'T');
    for (unsigned i = 0; i < 64u; ++i)
        tunuino_dual_transport_v2_step(&d, 100u, 0);
    assert(b.n == strlen(TUNUINO_TS_SIGNATURE));
    assert(a.n > 24u);
    assert(a.bytes[11] == 0x54u && a.bytes[14] == 3u);
    assert(!tunuino_dual_transport_v2_ts_receive(&d, 'W', 0));
    tunuino_dual_transport_v2_init(&d, ready, send, &a, ready, send, &a);
    assert(!d.configured);
    return 0;
}
