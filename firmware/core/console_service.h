// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_SERVICE_H
#define TUNUINO_CONSOLE_SERVICE_H
#include <stdint.h>
#include "console_tx.h"
#include "console_tx_queue.h"
#include "console_uart_pump.h"
typedef struct {
    tunuino_tx_queue_t queue;
    tunuino_console_tx_t transmitter;
    tunuino_uart_pump_t uart;
    uint32_t last_telemetry_ms;
    uint8_t hello_queued;
} tunuino_console_service_t;
void tunuino_console_service_init(tunuino_console_service_t *service,
    tunuino_uart_ready_fn ready, tunuino_uart_send_fn send, void *context);
// No telemetry is emitted unless caller explicitly supplies a valid snapshot.
// Caller must ensure data is real and validated before setting data_valid=1.
void tunuino_console_service_step(tunuino_console_service_t *service,
    uint32_t now_ms, const tunuino_console_telemetry_t *data, uint8_t data_valid);
#endif
