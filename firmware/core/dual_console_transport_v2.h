// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_DUAL_CONSOLE_TRANSPORT_V2_H
#define TUNUINO_DUAL_CONSOLE_TRANSPORT_V2_H
#include <stdint.h>
#include "console_partial_service.h"
#include "tunerstudio_buffered.h"
typedef struct {
    tunuino_console_service_t console;
    tunuino_ts_buffered_t tunerstudio;
    uint8_t configured;
} tunuino_dual_transport_v2_t;
// Board integration must guarantee distinct physical interfaces.
void tunuino_dual_transport_v2_init(tunuino_dual_transport_v2_t *d,
    tunuino_uart_ready_fn console_ready, tunuino_uart_send_fn console_send,
    void *console_context, tunuino_uart_ready_fn ts_ready,
    tunuino_uart_send_fn ts_send, void *ts_context);
void tunuino_dual_transport_v2_step(tunuino_dual_transport_v2_t *d,
    uint32_t now_ms, const tunuino_console_partial_t *partial);
uint8_t tunuino_dual_transport_v2_ts_receive(tunuino_dual_transport_v2_t *d,
    uint8_t command, const tunuino_ts_values_t *values);
#endif
