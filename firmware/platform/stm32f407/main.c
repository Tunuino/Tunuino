// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "board.h"
#include "startup_contract.h"
#include "mcu_clock.h"
#include "mcu_tick.h"
// Outputs remain electrically unconfigured and never armed.
int main(void) {
    tunuino_board_status_t board;
    tunuino_startup_t startup;
    tunuino_board_reset_status(&board);
    tunuino_startup_init(&startup);
    // Logical phase only: does not assert external driver safety.
    if (!tunuino_startup_advance(&startup, TUNUINO_STARTUP_OUTPUTS_SAFE)) {
        tunuino_startup_fault(&startup);
        for (;;) __asm volatile ("wfi");
    }
    if (!tunuino_clock_init_hsi()) {
        tunuino_startup_fault(&startup);
        for (;;) __asm volatile ("wfi");
    }
    (void)tunuino_startup_advance(&startup, TUNUINO_STARTUP_CLOCKS_READY);
    tunuino_tick_init();
    // Peripherals and power output stages are not initialized.
    for (;;) __asm volatile ("wfi");
}
