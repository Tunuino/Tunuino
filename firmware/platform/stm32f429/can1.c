// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "can1.h"
tunuino_can1_timing_t tunuino_can1_make_timing(uint32_t apb1_hz, uint32_t bitrate) {
    tunuino_can1_timing_t result = {0};
    if (!apb1_hz || !bitrate) return result;
    // Sync segment = 1 TQ, BS1 = 13 TQ, BS2 = 2 TQ, SJW = 1 TQ.
    // Only exact divisors are accepted: no silent bitrate approximation.
    const uint32_t divisor = bitrate * 16u;
    if (bitrate > UINT32_MAX / 16u || !divisor ||
        apb1_hz % divisor != 0u) return result;
    const uint32_t prescaler = apb1_hz / divisor;
    if (!prescaler || prescaler > 1024u) return result;
    result.bitrate = bitrate;
    result.prescaler = (uint16_t)prescaler;
    result.btr = (12u << 16) | (1u << 20) | (prescaler - 1u);
    result.valid = 1u;
    return result;
}
