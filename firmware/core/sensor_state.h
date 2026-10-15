// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_SENSOR_STATE_H
#define TUNUINO_SENSOR_STATE_H
#include <stdint.h>
// Host-testable sensor state. No actuator control or peripheral access.
typedef struct {
    uint16_t lambda_milli;
    uint32_t updated_ms;
    uint8_t valid;
} tunuino_lambda_state_t;
void tunuino_lambda_state_init(tunuino_lambda_state_t *state);
void tunuino_lambda_state_update(tunuino_lambda_state_t *state,
                                 uint16_t lambda_milli, uint32_t now_ms);
uint8_t tunuino_lambda_state_read(const tunuino_lambda_state_t *state,
                                  uint32_t now_ms, uint32_t timeout_ms,
                                  uint16_t *lambda_milli);
#endif
