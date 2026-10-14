// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "tunerstudio_buffered.h"
void tunuino_ts_buffered_init(tunuino_ts_buffered_t *s,
    tunuino_uart_ready_fn ready, tunuino_uart_send_fn send, void *context) {
    if (!s) return;
    *s = (tunuino_ts_buffered_t){0};
    tunuino_tx_queue_init(&s->queue);
    tunuino_uart_pump_init(&s->pump, ready, send, context);
}
uint8_t tunuino_ts_buffered_receive(tunuino_ts_buffered_t *s,
    uint8_t command, const tunuino_ts_values_t *values) {
    if (!s || !s->pump.ready || !s->pump.send) return 0u;
    uint8_t reply[32];
    const size_t count = tunuino_ts_reply(command, values, reply, sizeof(reply));
    if (!count || tunuino_tx_queue_write(reply, count, &s->queue) != count) {
        ++s->rejected;
        return 0u;
    }
    ++s->accepted;
    return 1u;
}
uint8_t tunuino_ts_buffered_pump(tunuino_ts_buffered_t *s) {
    return s ? tunuino_uart_pump_once(&s->pump, &s->queue) : 0u;
}
