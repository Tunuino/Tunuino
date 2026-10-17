// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "../firmware/core/output_runtime.h"
#include <assert.h>
int main(void) {
    tunuino_output_runtime_t s;
    const tunuino_output_port_t ports[] = {{TUNUINO_CAP_DIGITAL|TUNUINO_CAP_PWM,0},{TUNUINO_CAP_STEPPER,0}};
    tunuino_output_assignment_t a[] = {{0,TUNUINO_OUT_PWM,100},{1,TUNUINO_OUT_STEPPER,0}};
    tunuino_output_runtime_init(&s);
    assert(s.count == 0u && s.revision == 0u);
    assert(!tunuino_output_runtime_commit(&s,ports,2,a,2,0,1));
    assert(!tunuino_output_runtime_commit(&s,ports,2,a,2,1,0));
    assert(s.count == 0u);
    assert(tunuino_output_runtime_commit(&s,ports,2,a,2,1,1));
    assert(s.count == 2u && s.revision == 1u && s.assignments[0].pwm_hz == 100u);
    a[1].port=0;
    assert(!tunuino_output_runtime_commit(&s,ports,2,a,2,1,1));
    assert(s.count == 2u && s.revision == 1u && s.assignments[1].port == 1u);
    a[1].port=1;
    assert(!tunuino_output_runtime_commit(&s,ports,2,a,13,1,1));
    assert(tunuino_output_runtime_commit(&s,ports,2,a,1,1,1));
    assert(s.count == 1u && s.assignments[1].mode == TUNUINO_OUT_DISABLED);
    s.revision = UINT32_MAX;
    assert(!tunuino_output_runtime_commit(&s,ports,2,a,1,1,1));
    assert(s.revision == UINT32_MAX && s.count == 1u);
    return 0;
}
