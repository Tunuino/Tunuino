// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_DUAL_CONSOLE_TRANSPORT_H
#define TUNUINO_DUAL_CONSOLE_TRANSPORT_H
#include <stdint.h>
#include "console_service.h"
#include "tunerstudio_serial.h"
// Two dedicated, independently provisioned transports.
// Never multiplex both protocols over one byte stream.
typedef struct {
    tunuino_console_service_t console;
    tunuino_ts_serial_t tunerstudio;
    uint8_t configured;
} tunuino_dual_transport_t;
void tunuino_dual_transport_init(tunuino_dual_transport_t *transport,
    tunuino_uart_ready_fn console_ready, tunuino_uart_send_fn console_send,
    void *console_context, tunuino_ts_write_fn ts_write, void *ts_context);
// Explicit TunerStudio RX entry point. Does not parse Console frames.
uint8_t tunuino_dual_transport_ts_receive(tunuino_dual_transport_t *transport,
    uint8_t command, const tunuino_ts_values_t *values);
void tunuino_dual_transport_console_step(tunuino_dual_transport_t *transport,
    uint32_t now_ms, const tunuino_console_telemetry_t *snapshot, uint8_t valid);
#endif
