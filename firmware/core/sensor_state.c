// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "sensor_state.h"
void tunuino_lambda_state_init(tunuino_lambda_state_t *state) {
    if (!state) return;
    state->lambda_milli = 0u;
    state->updated_ms = 0u;
    state->valid = 0u;
}
void tunuino_lambda_state_update(tunuino_lambda_state_t *state,
                                 uint16_t lambda_milli, uint32_t now_ms) {
    if (!state) return;
    if (lambda_milli == 0u || lambda_milli == 0xffffu) {
        state->valid = 0u;
        state->lambda_milli = 0u;
        return;
    }
    state->lambda_milli = lambda_milli;
    state->updated_ms = now_ms;
    state->valid = 1u;
}
uint8_t tunuino_lambda_state_read(const tunuino_lambda_state_t *state,
                                  uint32_t now_ms, uint32_t timeout_ms,
                                  uint16_t *lambda_milli) {
    if (!lambda_milli) return 0u;
    *lambda_milli = 0u;
    if (!state || !state->valid || timeout_ms == 0u ||
        (uint32_t)(now_ms - state->updated_ms) > timeout_ms) return 0u;
    *lambda_milli = state->lambda_milli;
    return 1u;
}
