// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "console_tune_link.h"
#include <string.h>
void tunuino_console_tune_link_init(tunuino_console_tune_link_t *link,
    const tunuino_tune_wire_t *wire, tunuino_uart_ready_fn ready,
    tunuino_uart_send_fn send, void *context) {
    if (!link) return;
    memset(link, 0, sizeof(*link));
    link->wire = wire;
    tunuino_console_tune_rx_init(&link->rx);
    tunuino_tx_queue_init(&link->tx);
    tunuino_uart_pump_init(&link->pump, ready, send, context);
}
void tunuino_console_tune_link_rx(tunuino_console_tune_link_t *link, uint8_t byte) {
    if (link) tunuino_console_tune_rx_byte(&link->rx, byte, link->wire, &link->tx);
}
uint8_t tunuino_console_tune_link_step(tunuino_console_tune_link_t *link) {
    return link ? tunuino_uart_pump_once(&link->pump, &link->tx) : 0u;
}
