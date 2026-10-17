// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "output_preset.h"
const tunuino_output_port_t tunuino_auxiliary_draft_ports[TUNUINO_MAX_AUX_OUTPUTS] = {
    {TUNUINO_CAP_DIGITAL,1u}, {TUNUINO_CAP_DIGITAL,1u},
    {TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,1u}, {TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,1u},
    {TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,1u}, {TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,1u},
    {TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,1u},
    {TUNUINO_CAP_DIGITAL,1u}, {TUNUINO_CAP_DIGITAL,1u},
    {TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,1u}, {TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,1u},
    {TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,1u}
};
uint8_t tunuino_output_preset_validate(const tunuino_output_assignment_t *requested, uint8_t count) {
    return tunuino_output_validate(tunuino_auxiliary_draft_ports,TUNUINO_MAX_AUX_OUTPUTS,requested,count);
}
