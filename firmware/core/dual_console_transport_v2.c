// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "dual_console_transport_v2.h"
void tunuino_dual_transport_v2_init(tunuino_dual_transport_v2_t *d,
    tunuino_uart_ready_fn console_ready, tunuino_uart_send_fn console_send,
    void *console_context, tunuino_uart_ready_fn ts_ready,
    tunuino_uart_send_fn ts_send, void *ts_context) {
    if (!d) return;
    *d = (tunuino_dual_transport_v2_t){0};
    if (!console_ready || !console_send || !ts_ready || !ts_send ||
        (console_context && console_context == ts_context)) return;
    tunuino_console_service_init(&d->console, console_ready, console_send, console_context);
    tunuino_ts_buffered_init(&d->tunerstudio, ts_ready, ts_send, ts_context);
    d->configured = 1u;
}
void tunuino_dual_transport_v2_step(tunuino_dual_transport_v2_t *d,
    uint32_t now_ms, const tunuino_console_partial_t *partial) {
    if (!d || !d->configured) return;
    tunuino_console_partial_service_step(&d->console, now_ms, partial);
    (void)tunuino_ts_buffered_pump(&d->tunerstudio);
}
uint8_t tunuino_dual_transport_v2_ts_receive(tunuino_dual_transport_v2_t *d,
    uint8_t command, const tunuino_ts_values_t *values) {
    return d && d->configured ?
        tunuino_ts_buffered_receive(&d->tunerstudio, command, values) : 0u;
}
