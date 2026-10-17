// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#ifndef TUNUINO_OUTPUT_RUNTIME_H
#define TUNUINO_OUTPUT_RUNTIME_H
#include "output_config.h"
#define TUNUINO_MAX_AUX_OUTPUTS 12u
typedef struct {
    tunuino_output_assignment_t assignments[TUNUINO_MAX_AUX_OUTPUTS];
    uint8_t count;
    uint32_t revision;
} tunuino_output_runtime_t;
void tunuino_output_runtime_init(tunuino_output_runtime_t *runtime);
// Commit only while stopped and disarmed; no GPIO is touched by this module.
uint8_t tunuino_output_runtime_commit(tunuino_output_runtime_t *runtime,
    const tunuino_output_port_t *ports, uint8_t port_count,
    const tunuino_output_assignment_t *requested, uint8_t count,
    uint8_t engine_stopped, uint8_t outputs_disarmed);
#endif
