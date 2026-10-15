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
