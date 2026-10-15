// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "wideband_can.h"
uint8_t tunuino_wideband_decode(const tunuino_can1_frame_t *frame,
                                tunuino_wideband_sample_t *sample) {
    uint16_t lambda;
    if (sample == 0) return 0u;
    sample->lambda_milli = 0u;
    sample->counter = 0u;
    sample->valid = 0u;
    if (!tunuino_can1_frame_valid(frame) || frame->extended != 0u ||
        frame->identifier != TUNUINO_WIDEBAND_CAN_ID || frame->length != 4u ||
        frame->data[2] != 0u) return 0u;
    lambda = (uint16_t)((uint16_t)frame->data[0] | ((uint16_t)frame->data[1] << 8));
    if (lambda == 0u || lambda == 0xffffu) return 0u;
    sample->lambda_milli = lambda;
    sample->counter = frame->data[3];
    sample->valid = 1u;
    return 1u;
}

void tunuino_wideband_monitor_init(tunuino_wideband_monitor_t *monitor) {
    if (monitor == 0) return;
    monitor->sample.lambda_milli = 0u;
    monitor->sample.counter = 0u;
    monitor->sample.valid = 0u;
    monitor->last_received_ms = 0u;
    monitor->received = 0u;
}
uint8_t tunuino_wideband_monitor_receive(tunuino_wideband_monitor_t *monitor,
    const tunuino_can1_frame_t *frame, uint32_t now_ms) {
    tunuino_wideband_sample_t sample;
    if (monitor == 0 || !tunuino_wideband_decode(frame, &sample)) return 0u;
    monitor->sample = sample;
    monitor->last_received_ms = now_ms;
    monitor->received = 1u;
    return 1u;
}
uint8_t tunuino_wideband_monitor_fresh(const tunuino_wideband_monitor_t *monitor,
    uint32_t now_ms, uint32_t timeout_ms) {
    if (monitor == 0 || !monitor->received || !monitor->sample.valid || timeout_ms == 0u)
        return 0u;
    // Unsigned subtraction supports the 32-bit millisecond clock wrapping.
    return (uint8_t)((uint32_t)(now_ms - monitor->last_received_ms) <= timeout_ms);
}
