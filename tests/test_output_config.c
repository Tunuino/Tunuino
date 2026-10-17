// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "../firmware/core/output_config.h"
#include <assert.h>
int main(void) {
    const tunuino_output_port_t ports[]={{TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,0},{TUNUINO_CAP_STEPPER,0},{TUNUINO_CAP_PWM,1}};
    tunuino_output_assignment_t a[]={{0,TUNUINO_OUT_PWM,100},{1,TUNUINO_OUT_STEPPER,0}};
    assert(tunuino_output_validate(ports,3,a,2));
    a[1].port=0; assert(!tunuino_output_validate(ports,3,a,2));
    a[1].port=1; a[1].mode=TUNUINO_OUT_PWM; a[1].pwm_hz=100; assert(!tunuino_output_validate(ports,3,a,2));
    a[1].port=2; assert(!tunuino_output_validate(ports,3,a,2));
    a[1].port=1; a[1].mode=TUNUINO_OUT_STEPPER; a[1].pwm_hz=0;
    a[0].pwm_hz=0; assert(!tunuino_output_validate(ports,3,a,2));
    a[0].port=0; a[0].mode=TUNUINO_OUT_DIGITAL; a[0].pwm_hz=0;
    a[1].port=1; a[1].mode=TUNUINO_OUT_DISABLED; a[1].pwm_hz=0;
    assert(tunuino_output_validate(ports,3,a,2));
    a[0].mode=(tunuino_output_mode_t)99;
    assert(!tunuino_output_validate(ports,3,a,2));
    a[0].mode=TUNUINO_OUT_DIGITAL;
    assert(!tunuino_output_validate(ports,0,a,2));
    assert(!tunuino_output_validate(0,3,a,2));
    assert(!tunuino_output_validate(ports,3,0,2));
    return 0;
}
