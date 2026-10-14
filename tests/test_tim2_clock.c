// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/platform/stm32f407/tim2_capture.h"
#include <assert.h>
int main(void) {
    tunuino_tim2_clock_config_t cfg = tunuino_tim2_make_1mhz_config(16000000u);
    assert(cfg.valid && cfg.prescaler == 15u);
    cfg = tunuino_tim2_make_1mhz_config(84000000u);
    assert(cfg.valid && cfg.prescaler == 83u);
    cfg = tunuino_tim2_make_1mhz_config(16000001u);
    assert(!cfg.valid);
    cfg = tunuino_tim2_make_1mhz_config(500000u);
    assert(!cfg.valid);
    cfg = tunuino_tim2_make_1mhz_config(0u);
    assert(!cfg.valid);
    return 0;
}
