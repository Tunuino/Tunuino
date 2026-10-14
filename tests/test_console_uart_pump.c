// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_uart_pump.h"
#include <assert.h>
typedef struct {uint8_t ready; uint8_t bytes[8]; unsigned count;} fake_uart_t;
static uint8_t ready(void *ctx) {return ((fake_uart_t *)ctx)->ready;}
static void send(uint8_t b, void *ctx) {
    fake_uart_t *uart = ctx;
    assert(uart->count < sizeof(uart->bytes));
    uart->bytes[uart->count++] = b;
}
int main(void) {
    tunuino_tx_queue_t queue;
    tunuino_tx_queue_init(&queue);
    const uint8_t data[] = {0x54u, 0x4eu, 0x01u};
    assert(tunuino_tx_queue_write(data, sizeof(data), &queue) == sizeof(data));
    fake_uart_t fake = {0u, {0}, 0u};
    tunuino_uart_pump_t pump;
    tunuino_uart_pump_init(&pump, ready, send, &fake);
    assert(!tunuino_uart_pump_once(&pump, &queue));
    assert(tunuino_tx_queue_size(&queue) == 3u);
    fake.ready = 1u;
    assert(tunuino_uart_pump_once(&pump, &queue));
    assert(fake.bytes[0] == 0x54u && tunuino_tx_queue_size(&queue) == 2u);
    assert(tunuino_uart_pump_once(&pump, &queue));
    assert(tunuino_uart_pump_once(&pump, &queue));
    assert(!tunuino_uart_pump_once(&pump, &queue));
    assert(fake.count == 3u && pump.bytes_sent == 3u);
    return 0;
}
