// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/platform/stm32f429/can1.h"
#include <assert.h>
int main(void) {
    tunuino_can1_timing_t t = tunuino_can1_make_timing(42000000u, 500000u);
    assert(!t.valid); // 42 MHz cannot make exact 500 kbit/s with 16 TQ
    t = tunuino_can1_make_timing(16000000u, 500000u);
    assert(t.valid && t.prescaler == 2u && t.btr == ((12u << 16) | (1u << 20) | 1u));
    t = tunuino_can1_make_timing(16000000u, 250000u);
    assert(t.valid && t.prescaler == 4u);
    assert(!tunuino_can1_make_timing(0u, 500000u).valid);
    assert(!tunuino_can1_make_timing(16000000u, 0u).valid);
    assert(!tunuino_can1_make_timing(16000000u, 400000000u).valid);
    /* Validate the CAN transport boundary before any hardware TX is added. */
    tunuino_can1_frame_t frame = {0};
    assert(tunuino_can1_frame_valid(&frame));
    frame.identifier = 0x7ffu;
    frame.length = 8u;
    assert(tunuino_can1_frame_valid(&frame));
    frame.identifier = 0x800u;
    assert(!tunuino_can1_frame_valid(&frame));
    frame.extended = 1u;
    assert(tunuino_can1_frame_valid(&frame));
    frame.identifier = 0x1fffffffu;
    assert(tunuino_can1_frame_valid(&frame));
    frame.identifier++;
    assert(!tunuino_can1_frame_valid(&frame));
    frame.identifier = 0u;
    frame.length = 9u;
    assert(!tunuino_can1_frame_valid(&frame));
    frame.length = 8u;
    frame.remote = 1u;
    assert(!tunuino_can1_frame_valid(&frame));
    frame.remote = 0u;
    frame.extended = 2u;
    assert(!tunuino_can1_frame_valid(&frame));
    assert(!tunuino_can1_frame_valid(0));
    return 0;
}
