// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TUNERSTUDIO_BUFFERED_H
#define TUNUINO_TUNERSTUDIO_BUFFERED_H
#include <stdint.h>
#include "console_tx_queue.h"
#include "console_uart_pump.h"
#include "tunerstudio_protocol.h"
typedef struct {
    tunuino_tx_queue_t queue;
    tunuino_uart_pump_t pump;
    uint32_t accepted;
    uint32_t rejected;
} tunuino_ts_buffered_t;
void tunuino_ts_buffered_init(tunuino_ts_buffered_t *s,
    tunuino_uart_ready_fn ready, tunuino_uart_send_fn send, void *context);
uint8_t tunuino_ts_buffered_receive(tunuino_ts_buffered_t *s,
    uint8_t command, const tunuino_ts_values_t *values);
uint8_t tunuino_ts_buffered_pump(tunuino_ts_buffered_t *s);
#endif
