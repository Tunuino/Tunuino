// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "board.h"
#include "startup_contract.h"
// First bare-metal image: no GPIO is configured and no output drivers are enabled.
// A separate hardware interlock is required to guarantee safe power-up.
int main(void) {
    tunuino_board_status_t board;
    tunuino_startup_t startup;
    tunuino_board_reset_status(&board);
    tunuino_startup_init(&startup);
    // No transition to OUTPUTS_SAFE until board-level safe GPIO states are verified.
    for (;;) __asm volatile ("wfi");
}
