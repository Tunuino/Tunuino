// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/sensor_state.h"
#include <assert.h>
int main(void) {
    tunuino_lambda_state_t state;
    uint16_t lambda = 777u;
    tunuino_lambda_state_init(&state);
    assert(!tunuino_lambda_state_read(&state, 0u, 100u, &lambda));
    assert(lambda == 0u);
    tunuino_lambda_state_update(&state, 1020u, 1000u);
    assert(tunuino_lambda_state_read(&state, 1100u, 100u, &lambda));
    assert(lambda == 1020u);
    assert(!tunuino_lambda_state_read(&state, 1101u, 100u, &lambda));
    assert(lambda == 0u);
    tunuino_lambda_state_update(&state, 0u, 1200u);
    assert(!tunuino_lambda_state_read(&state, 1200u, 100u, &lambda));
    tunuino_lambda_state_update(&state, 1000u, 0xfffffff0u);
    assert(tunuino_lambda_state_read(&state, 0x10u, 40u, &lambda));
    assert(!tunuino_lambda_state_read(&state, 0x30u, 40u, &lambda));
    assert(!tunuino_lambda_state_read(&state, 0u, 0u, &lambda));
    assert(!tunuino_lambda_state_read(0, 0u, 100u, &lambda));
    assert(!tunuino_lambda_state_read(&state, 0u, 100u, 0));
    return 0;
}
