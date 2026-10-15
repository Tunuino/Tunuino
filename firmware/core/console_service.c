// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "console_service.h"
void tunuino_console_service_init(tunuino_console_service_t *service,
    tunuino_uart_ready_fn ready, tunuino_uart_send_fn send, void *context) {
    if (!service) return;
    *service = (tunuino_console_service_t){0};
    tunuino_tx_queue_init(&service->queue);
    tunuino_console_tx_init(&service->transmitter, tunuino_tx_queue_write, &service->queue);
    tunuino_uart_pump_init(&service->uart, ready, send, context);
}
void tunuino_console_service_step(tunuino_console_service_t *service,
    uint32_t now_ms, const tunuino_console_telemetry_t *data, uint8_t data_valid) {
    if (!service) return;
    // Queue HELLO only when an actual transport callback is configured.
    if (!service->hello_queued && service->uart.ready && service->uart.send) {
        if (tunuino_console_tx_hello(&service->transmitter)) service->hello_queued = 1u;
    }
    // 10 Hz maximum; no fabricated sensor data and no automatic enablement.
    if (service->hello_queued && data_valid && data &&
        (uint32_t)(now_ms - service->last_telemetry_ms) >= 100u) {
        if (tunuino_console_tx_telemetry(&service->transmitter, data))
            service->last_telemetry_ms = now_ms;
    }
    (void)tunuino_uart_pump_once(&service->uart, &service->queue);
}
