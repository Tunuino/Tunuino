// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_TUNE_LINK_H
#define TUNUINO_CONSOLE_TUNE_LINK_H
#include "console_tune_rx.h"
#include "console_uart_pump.h"
// Host-testable, read-only transport. One polling context; no IRQ concurrency.
typedef struct {
    tunuino_console_tune_rx_t rx;
    tunuino_tx_queue_t tx;
    tunuino_uart_pump_t pump;
    const tunuino_tune_wire_t *wire;
} tunuino_console_tune_link_t;
void tunuino_console_tune_link_init(tunuino_console_tune_link_t *link,
    const tunuino_tune_wire_t *wire, tunuino_uart_ready_fn ready,
    tunuino_uart_send_fn send, void *context);
void tunuino_console_tune_link_rx(tunuino_console_tune_link_t *link, uint8_t byte);
uint8_t tunuino_console_tune_link_step(tunuino_console_tune_link_t *link);
#endif
