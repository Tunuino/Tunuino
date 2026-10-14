// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_UART_PUMP_H
#define TUNUINO_CONSOLE_UART_PUMP_H
#include <stdint.h>
#include "console_tx_queue.h"
// Abstract UART sink: ready returns 1 only when peripheral accepts a byte.
// send writes exactly one byte to the peripheral.
typedef uint8_t (*tunuino_uart_ready_fn)(void *context);
typedef void (*tunuino_uart_send_fn)(uint8_t byte, void *context);
typedef struct {
    tunuino_uart_ready_fn ready;
    tunuino_uart_send_fn send;
    void *context;
    uint32_t bytes_sent;
} tunuino_uart_pump_t;
void tunuino_uart_pump_init(tunuino_uart_pump_t *pump,
                            tunuino_uart_ready_fn ready,
                            tunuino_uart_send_fn send, void *context);
// Single-context polling only: no concurrent queue producer/consumer allowed.
uint8_t tunuino_uart_pump_once(tunuino_uart_pump_t *pump, tunuino_tx_queue_t *queue);
#endif
