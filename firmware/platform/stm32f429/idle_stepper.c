// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "idle_stepper.h"
void tunuino_idle_stepper_init(tunuino_idle_stepper_t *motor) {
    if (motor == 0) return;
    motor->hardware_verified = 0u;
    motor->fault_latched = 0u;
    motor->driver_enabled = 0u;
    motor->target_valid = 0u;
    motor->target_steps = 0;
}
void tunuino_idle_stepper_fault(tunuino_idle_stepper_t *motor) {
    if (motor == 0) return;
    motor->fault_latched = 1u;
    motor->driver_enabled = 0u;
    motor->target_valid = 0u;
}
uint8_t tunuino_idle_stepper_set_target(tunuino_idle_stepper_t *motor, int32_t target) {
    if (motor == 0 || motor->fault_latched || !motor->hardware_verified) return 0u;
    motor->target_steps = target;
    motor->target_valid = 1u;
    return 1u;
}
uint8_t tunuino_idle_stepper_outputs_allowed(const tunuino_idle_stepper_t *motor) {
    return (uint8_t)(motor != 0 && motor->hardware_verified == 1u &&
        motor->fault_latched == 0u && motor->driver_enabled == 1u &&
        motor->target_valid == 1u);
}
