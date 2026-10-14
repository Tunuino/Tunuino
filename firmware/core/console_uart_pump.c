// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_uart_pump.h"
void tunuino_uart_pump_init(tunuino_uart_pump_t *pump,
                            tunuino_uart_ready_fn ready,
                            tunuino_uart_send_fn send, void *context) {
    if (!pump) return;
    *pump = (tunuino_uart_pump_t){0};
    pump->ready = ready;
    pump->send = send;
    pump->context = context;
}
uint8_t tunuino_uart_pump_once(tunuino_uart_pump_t *pump, tunuino_tx_queue_t *queue) {
    if (!pump || !queue || !pump->ready || !pump->send || !pump->ready(pump->context))
        return 0u;
    uint8_t byte;
    if (!tunuino_tx_queue_pop(queue, &byte)) return 0u;
    pump->send(byte, pump->context);
    if (pump->bytes_sent != UINT32_MAX) ++pump->bytes_sent;
    return 1u;
}
