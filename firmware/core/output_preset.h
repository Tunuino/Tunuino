// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#ifndef TUNUINO_OUTPUT_PRESET_H
#define TUNUINO_OUTPUT_PRESET_H
#include "output_runtime.h"
// Connector-only provisional capability map. All ports are reserved until hardware review.
extern const tunuino_output_port_t tunuino_auxiliary_draft_ports[TUNUINO_MAX_AUX_OUTPUTS];
uint8_t tunuino_output_preset_validate(const tunuino_output_assignment_t *requested, uint8_t count);
#endif
