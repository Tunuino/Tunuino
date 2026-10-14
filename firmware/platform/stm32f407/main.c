// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "board.h"
#include "startup_contract.h"
#include "mcu_clock.h"
#include "mcu_tick.h"
#include "usart_console.h"
#include "../../core/console_tune_link.h"
#include "../../core/console_protocol.h"
#include "../../core/tune_wire.h"

// Read-only communication service. USART2 remains electrically disconnected:
// no GPIO alternate function, UE/TE/RE or interrupts are enabled.
static tunuino_console_tune_link_t console_link;
static tunuino_tune_wire_t tune_wire;
static uint8_t service_ready;

static void communication_init(void) {
    const tunuino_usart_config_t config = tunuino_usart_make_config(16000000u, 115200u);
    if (!tunuino_usart2_init_disconnected(&config)) return;
    tunuino_console_tune_link_init(&console_link, &tune_wire,
        tunuino_usart2_tx_ready, tunuino_usart2_send_byte, 0);
    // A valid tune snapshot must be explicitly provisioned before any map
    // request can be answered. Never fabricate live ECU maps.
    uint8_t hello[11];
    const uint8_t identity[3] = {0x54u, 0x4eu, TUNUINO_CONSOLE_PROTOCOL_VERSION};
    const size_t n = tunuino_console_encode(TUNUINO_CONSOLE_TYPE_HELLO,
        identity, sizeof(identity), hello, sizeof(hello));
    if (n) (void)tunuino_tx_queue_write(hello, n, &console_link.tx);
    service_ready = 1u;
}
int main(void) {
    tunuino_board_status_t board;
    tunuino_startup_t startup;
    tunuino_board_reset_status(&board);
    tunuino_startup_init(&startup);
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
    communication_init();
    for (;;) {
        if (service_ready) (void)tunuino_console_tune_link_step(&console_link);
        __asm volatile ("wfi");
    }
}
