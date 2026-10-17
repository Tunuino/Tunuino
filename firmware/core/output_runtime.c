// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "output_runtime.h"
void tunuino_output_runtime_init(tunuino_output_runtime_t *runtime) {
    if (runtime == 0) return;
    runtime->count = 0u;
    runtime->revision = 0u;
    for (uint8_t i = 0u; i < TUNUINO_MAX_AUX_OUTPUTS; ++i) {
        runtime->assignments[i].port = 0u;
        runtime->assignments[i].mode = TUNUINO_OUT_DISABLED;
        runtime->assignments[i].pwm_hz = 0u;
    }
}
uint8_t tunuino_output_runtime_commit(tunuino_output_runtime_t *runtime,
    const tunuino_output_port_t *ports, uint8_t port_count,
    const tunuino_output_assignment_t *requested, uint8_t count,
    uint8_t engine_stopped, uint8_t outputs_disarmed) {
    if (runtime == 0 || runtime->revision == UINT32_MAX || count > TUNUINO_MAX_AUX_OUTPUTS ||
        engine_stopped != 1u || outputs_disarmed != 1u ||
        !tunuino_output_validate(ports, port_count, requested, count)) return 0u;
    // No partial update on validation failure.
    for (uint8_t i = 0u; i < count; ++i) runtime->assignments[i] = requested[i];
    for (uint8_t i = count; i < TUNUINO_MAX_AUX_OUTPUTS; ++i) {
        runtime->assignments[i].port = 0u;
        runtime->assignments[i].mode = TUNUINO_OUT_DISABLED;
        runtime->assignments[i].pwm_hz = 0u;
    }
    runtime->count = count;
    runtime->revision++;
    return 1u;
}
