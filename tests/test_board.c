// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/platform/stm32f407/board.h"
#include <assert.h>
int main(void) {
    tunuino_board_status_t status;
    tunuino_board_reset_status(&status);
    assert(status.state == TUNUINO_HW_UNINITIALIZED);
    assert(tunuino_board_outputs_allowed(&status) == 0u);
    status.state = TUNUINO_HW_SAFE;
    assert(tunuino_board_outputs_allowed(&status) == 0u);
    status.outputs_armed = 1u;
    assert(tunuino_board_outputs_allowed(&status) == 1u);
    status.state = TUNUINO_HW_FAULT;
    assert(tunuino_board_outputs_allowed(&status) == 0u);
    assert(tunuino_board_outputs_allowed(0) == 0u);
    return 0;
}
