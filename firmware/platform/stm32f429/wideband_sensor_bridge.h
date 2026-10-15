// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_WIDEBAND_SENSOR_BRIDGE_H
#define TUNUINO_WIDEBAND_SENSOR_BRIDGE_H
#include <stdint.h>
#include "wideband_can.h"
#include "../../core/sensor_state.h"
// Transport-to-sensor adapter. Caller provides received CAN frames and time.
typedef struct {
    tunuino_wideband_monitor_t monitor;
    tunuino_lambda_state_t lambda;
} tunuino_wideband_sensor_bridge_t;
void tunuino_wideband_sensor_bridge_init(tunuino_wideband_sensor_bridge_t *bridge);
uint8_t tunuino_wideband_sensor_bridge_receive(tunuino_wideband_sensor_bridge_t *bridge,
    const tunuino_can1_frame_t *frame, uint32_t now_ms);
uint8_t tunuino_wideband_sensor_bridge_read(const tunuino_wideband_sensor_bridge_t *bridge,
    uint32_t now_ms, uint32_t timeout_ms, uint16_t *lambda_milli);
#endif
