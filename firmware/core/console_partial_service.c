// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_partial_service.h"
void tunuino_console_partial_service_step(tunuino_console_service_t *service,
    uint32_t now_ms, const tunuino_console_partial_t *snapshot) {
    if (!service) return;
    if (!service->hello_queued && service->uart.ready && service->uart.send) {
        if (tunuino_console_tx_hello(&service->transmitter)) service->hello_queued = 1u;
    }
    if (service->hello_queued && snapshot &&
        (uint32_t)(now_ms - service->last_telemetry_ms) >= 100u) {
        uint8_t frame[24];
        const size_t size = tunuino_console_encode_partial(snapshot, frame, sizeof(frame));
        if (size && tunuino_tx_queue_write(frame, size, &service->queue) == size)
            service->last_telemetry_ms = now_ms;
    }
    (void)tunuino_uart_pump_once(&service->uart, &service->queue);
}
