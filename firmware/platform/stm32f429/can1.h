// SPDX-License-Identifier: MIT
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
#endif
