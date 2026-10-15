// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "dual_console_transport.h"
void tunuino_dual_transport_init(tunuino_dual_transport_t *transport,
    tunuino_uart_ready_fn console_ready, tunuino_uart_send_fn console_send,
    void *console_context, tunuino_ts_write_fn ts_write, void *ts_context) {
    if (!transport) return;
    *transport = (tunuino_dual_transport_t){0};
    // A missing endpoint leaves both disabled, preventing partial setup.
    if (!console_ready || !console_send || !ts_write ||
        (console_context && console_context == ts_context)) return;
    tunuino_console_service_init(&transport->console,
        console_ready, console_send, console_context);
    tunuino_ts_serial_init(&transport->tunerstudio, ts_write, ts_context);
    transport->configured = 1u;
}
uint8_t tunuino_dual_transport_ts_receive(tunuino_dual_transport_t *transport,
    uint8_t command, const tunuino_ts_values_t *values) {
    if (!transport || !transport->configured) return 0u;
    return tunuino_ts_serial_receive(&transport->tunerstudio, command, values);
}
void tunuino_dual_transport_console_step(tunuino_dual_transport_t *transport,
    uint32_t now_ms, const tunuino_console_telemetry_t *snapshot, uint8_t valid) {
    if (!transport || !transport->configured) return;
    tunuino_console_service_step(&transport->console, now_ms, snapshot, valid);
}
