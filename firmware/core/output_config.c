// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "output_config.h"
uint8_t tunuino_output_validate(const tunuino_output_port_t *ports, uint8_t port_count, const tunuino_output_assignment_t *assignments, uint8_t assignment_count) {
    if (ports == 0 || (assignments == 0 && assignment_count != 0u)) return 0u;
    for (uint8_t i=0u;i<assignment_count;i++) {
        const tunuino_output_assignment_t *a=&assignments[i];
        if (a->port >= port_count || ports[a->port].reserved) return 0u;
        uint8_t required=0u;
        switch (a->mode) {
            case TUNUINO_OUT_DISABLED: if (a->pwm_hz != 0u) return 0u; break;
            case TUNUINO_OUT_DIGITAL: required=TUNUINO_CAP_DIGITAL; if (a->pwm_hz != 0u) return 0u; break;
            case TUNUINO_OUT_PWM: required=TUNUINO_CAP_PWM; if (a->pwm_hz == 0u) return 0u; break;
            case TUNUINO_OUT_STEPPER: required=TUNUINO_CAP_STEPPER; if (a->pwm_hz != 0u) return 0u; break;
            default: return 0u;
        }
        if ((ports[a->port].capabilities & required) != required) return 0u;
        for (uint8_t j=0u;j<i;j++) if (assignments[j].port==a->port) return 0u;
    }
    return 1u;
}
