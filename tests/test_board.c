// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/platform/stm32f429/board.h"
#include <assert.h>
int main(void) {
    /* Pin contract: MCU assignments only, not verified connector wiring. */
    assert(TUNUINO_CONSOLE_USART == 2u);
    assert(TUNUINO_CONSOLE_TX_PORT == 'D' && TUNUINO_CONSOLE_TX_PIN == 5u);
    assert(TUNUINO_CONSOLE_RX_PORT == 'D' && TUNUINO_CONSOLE_RX_PIN == 6u);
    assert(TUNUINO_CONSOLE_AF == 7u);
    assert(TUNUINO_CAN_WIDEBAND_BUS == 1u);
    assert(TUNUINO_CAN1_RX_PORT == 'D' && TUNUINO_CAN1_RX_PIN == 0u);
    assert(TUNUINO_CAN1_TX_PORT == 'D' && TUNUINO_CAN1_TX_PIN == 1u);
    assert(TUNUINO_CAN1_AF == 9u);
    assert(TUNUINO_CONSOLE_TX_PIN != TUNUINO_CONSOLE_RX_PIN);
    assert(TUNUINO_CAN1_RX_PIN != TUNUINO_CAN1_TX_PIN);
    assert(TUNUINO_CONSOLE_TX_PIN != TUNUINO_CAN1_RX_PIN);
    assert(TUNUINO_CONSOLE_TX_PIN != TUNUINO_CAN1_TX_PIN);
    assert(TUNUINO_CONSOLE_RX_PIN != TUNUINO_CAN1_RX_PIN);
    assert(TUNUINO_CONSOLE_RX_PIN != TUNUINO_CAN1_TX_PIN);
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
