// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/platform/stm32f407/startup_contract.h"
#include <assert.h>
int main(void) {
    tunuino_startup_t state;
    tunuino_startup_init(&state);
    assert(state.phase == TUNUINO_STARTUP_RESET);
    assert(state.hardware_output_enable == 0u);
    assert(tunuino_startup_advance(&state, TUNUINO_STARTUP_PERIPHERALS_READY) == 0u);
    assert(tunuino_startup_advance(&state, TUNUINO_STARTUP_OUTPUTS_SAFE) == 1u);
    assert(tunuino_startup_advance(&state, TUNUINO_STARTUP_CLOCKS_READY) == 1u);
    assert(tunuino_startup_advance(&state, TUNUINO_STARTUP_PERIPHERALS_READY) == 1u);
    assert(state.hardware_output_enable == 0u);
    assert(tunuino_startup_advance(&state, TUNUINO_STARTUP_RESET) == 0u);
    tunuino_startup_fault(&state);
    assert(state.phase == TUNUINO_STARTUP_FAULT);
    assert(state.hardware_output_enable == 0u);
    assert(tunuino_startup_advance(&state, TUNUINO_STARTUP_OUTPUTS_SAFE) == 0u);
    return 0;
}
