// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "startup_contract.h"
void tunuino_startup_init(tunuino_startup_t *startup) {
    if (!startup) return;
    startup->phase = TUNUINO_STARTUP_RESET;
    startup->hardware_output_enable = 0u;
}
uint8_t tunuino_startup_advance(tunuino_startup_t *startup, tunuino_startup_phase_t next) {
    if (!startup || startup->phase == TUNUINO_STARTUP_FAULT) return 0u;
    if (next != (tunuino_startup_phase_t)(startup->phase + 1)) return 0u;
    if (next > TUNUINO_STARTUP_PERIPHERALS_READY) return 0u;
    startup->phase = next;
    // Output enable is never granted by the startup sequence.
    startup->hardware_output_enable = 0u;
    return 1u;
}
void tunuino_startup_fault(tunuino_startup_t *startup) {
    if (!startup) return;
    startup->phase = TUNUINO_STARTUP_FAULT;
    startup->hardware_output_enable = 0u;
}
