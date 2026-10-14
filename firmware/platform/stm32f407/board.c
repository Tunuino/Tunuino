// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "board.h"
void tunuino_board_reset_status(tunuino_board_status_t *status) {
    if (status == 0) return;
    status->state = TUNUINO_HW_UNINITIALIZED;
    status->outputs_armed = 0u;
}
uint8_t tunuino_board_outputs_allowed(const tunuino_board_status_t *status) {
    if (status == 0) return 0u;
    return (uint8_t)(status->state == TUNUINO_HW_SAFE && status->outputs_armed == 1u);
}
