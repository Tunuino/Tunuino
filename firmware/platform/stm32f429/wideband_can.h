// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_WIDEBAND_CAN_H
#define TUNUINO_WIDEBAND_CAN_H
#include <stdint.h>
#include "can1.h"

// Tunuino experimental wire format (not an external controller protocol).
// Standard CAN ID 0x520, DLC 4: lambda in 1/1000, status, rolling counter.
// Lambda 0 or 0xffff is invalid. Status 0 means measurement ready.
#define TUNUINO_WIDEBAND_CAN_ID 0x520u
typedef struct {
    uint16_t lambda_milli;
    uint8_t counter;
    uint8_t valid;
} tunuino_wideband_sample_t;
uint8_t tunuino_wideband_decode(const tunuino_can1_frame_t *frame,
                                tunuino_wideband_sample_t *sample);
#endif
