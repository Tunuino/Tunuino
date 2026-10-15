// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CAN1_H
#define TUNUINO_CAN1_H
#include <stdint.h>
// bxCAN bit timing with 16 time quanta per bit, 87.5% sample point.
// Pure calculation: does not touch peripheral registers or GPIO.
typedef struct {
    uint32_t bitrate;
    uint32_t btr;
    uint16_t prescaler;
    uint8_t valid;
} tunuino_can1_timing_t;
tunuino_can1_timing_t tunuino_can1_make_timing(uint32_t apb1_hz, uint32_t bitrate);
// Strict classic CAN frame contract for the external wideband transport.
// Data frames only; standard (11-bit) or extended (29-bit) identifiers.
typedef struct {
    uint32_t identifier;
    uint8_t extended;
    uint8_t remote;
    uint8_t length;
    uint8_t data[8];
} tunuino_can1_frame_t;
uint8_t tunuino_can1_frame_valid(const tunuino_can1_frame_t *frame);
#endif
