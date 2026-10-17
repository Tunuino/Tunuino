// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "../firmware/platform/stm32f429/idle_stepper.h"
#include <assert.h>
int main(void) {
    tunuino_idle_stepper_t s;
    tunuino_idle_stepper_init(&s);
    assert(!tunuino_idle_stepper_outputs_allowed(&s));
    assert(!tunuino_idle_stepper_set_target(&s, 100));
    s.hardware_verified = 1u;
    assert(tunuino_idle_stepper_set_target(&s, 100));
    assert(!tunuino_idle_stepper_outputs_allowed(&s));
    s.driver_enabled = 1u;
    assert(tunuino_idle_stepper_outputs_allowed(&s));
    tunuino_idle_stepper_disarm(&s);
    assert(!tunuino_idle_stepper_outputs_allowed(&s));
    assert(tunuino_idle_stepper_set_target(&s, 50));
    assert(!tunuino_idle_stepper_outputs_allowed(&s));
    s.driver_enabled = 1u;
    assert(tunuino_idle_stepper_outputs_allowed(&s));
    tunuino_idle_stepper_fault(&s);
    assert(!tunuino_idle_stepper_outputs_allowed(&s));
    assert(!tunuino_idle_stepper_set_target(&s, 20));
    tunuino_idle_stepper_init(&s);
    assert(!tunuino_idle_stepper_outputs_allowed(&s));
    return 0;
}
