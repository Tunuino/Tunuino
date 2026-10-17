// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "../firmware/core/output_preset.h"
#include <assert.h>
int main(void) {
    tunuino_output_assignment_t a={2u,TUNUINO_OUT_PWM,100u};
    tunuino_output_runtime_t state;
    tunuino_output_runtime_init(&state);
    assert(!tunuino_output_preset_validate(&a,1u));
    assert(tunuino_output_preset_validate(0,0u));
    assert(!tunuino_output_runtime_commit(&state,tunuino_auxiliary_draft_ports,TUNUINO_MAX_AUX_OUTPUTS,&a,1u,1u,1u));
    assert(state.revision == 0u && state.count == 0u);
    return 0;
}
