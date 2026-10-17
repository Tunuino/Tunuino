// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_IDLE_STEPPER_H
#define TUNUINO_IDLE_STEPPER_H
#include <stdint.h>
typedef struct {
    uint8_t hardware_verified;
    uint8_t fault_latched;
    uint8_t driver_enabled;
    uint8_t target_valid;
    int32_t target_steps;
} tunuino_idle_stepper_t;
void tunuino_idle_stepper_init(tunuino_idle_stepper_t *motor);
void tunuino_idle_stepper_disarm(tunuino_idle_stepper_t *motor);
void tunuino_idle_stepper_fault(tunuino_idle_stepper_t *motor);
uint8_t tunuino_idle_stepper_set_target(tunuino_idle_stepper_t *motor, int32_t target);
uint8_t tunuino_idle_stepper_outputs_allowed(const tunuino_idle_stepper_t *motor);
#endif
