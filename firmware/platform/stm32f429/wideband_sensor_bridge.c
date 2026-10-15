// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "wideband_sensor_bridge.h"
void tunuino_wideband_sensor_bridge_init(tunuino_wideband_sensor_bridge_t *bridge) {
    if (!bridge) return;
    tunuino_wideband_monitor_init(&bridge->monitor);
    tunuino_lambda_state_init(&bridge->lambda);
}
uint8_t tunuino_wideband_sensor_bridge_receive(tunuino_wideband_sensor_bridge_t *bridge,
    const tunuino_can1_frame_t *frame, uint32_t now_ms) {
    if (!bridge) return 0u;
    // Only the configured, correctly formed sender can change sensor state.
    // A replayed fault must not invalidate a newer valid measurement.
    if (tunuino_can1_frame_valid(frame) && !frame->extended &&
        frame->identifier == TUNUINO_WIDEBAND_CAN_ID && frame->length == 4u &&
        bridge->monitor.received &&
        bridge->monitor.sample.counter == frame->data[3]) return 0u;
    // An explicit not-ready or invalid measurement from our configured sender
    // revokes the old sample immediately. Ignore unrelated/malformed frames.
    if (tunuino_can1_frame_valid(frame) && !frame->extended &&
        frame->identifier == TUNUINO_WIDEBAND_CAN_ID && frame->length == 4u &&
        (frame->data[2] != 0u ||
         (frame->data[0] == 0u && frame->data[1] == 0u) ||
         (frame->data[0] == 0xffu && frame->data[1] == 0xffu))) {
        bridge->monitor.sample.valid = 0u;
        bridge->monitor.received = 0u;
        tunuino_lambda_state_init(&bridge->lambda);
        return 0u;
    }
    if (!tunuino_wideband_monitor_receive(&bridge->monitor, frame, now_ms))
        return 0u;
    tunuino_lambda_state_update(&bridge->lambda,
        bridge->monitor.sample.lambda_milli, now_ms);
    return 1u;
}
uint8_t tunuino_wideband_sensor_bridge_read(const tunuino_wideband_sensor_bridge_t *bridge,
    uint32_t now_ms, uint32_t timeout_ms, uint16_t *lambda_milli) {
    if (!lambda_milli) return 0u;
    *lambda_milli = 0u;
    if (!bridge || !tunuino_wideband_monitor_fresh(&bridge->monitor, now_ms, timeout_ms))
        return 0u;
    return tunuino_lambda_state_read(&bridge->lambda, now_ms, timeout_ms, lambda_milli);
}
