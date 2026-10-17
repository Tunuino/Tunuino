// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_USART_CONSOLE_H
#define TUNUINO_USART_CONSOLE_H
#include <stdint.h>
// USART2 reference pin mapping: PD5 (TX), PD6 (RX), AF7.\n// Verify STM32F429 board pin assignments before use; no GPIO, NVIC or UART enable performed.
typedef struct {
    uint32_t peripheral_clock_hz;
    uint32_t baud_rate;
    uint16_t brr;
    uint8_t valid;
} tunuino_usart_config_t;
tunuino_usart_config_t tunuino_usart_make_config(uint32_t pclk_hz, uint32_t baud);
uint8_t tunuino_usart2_init_disconnected(const tunuino_usart_config_t *config);
uint8_t tunuino_usart2_tx_ready(void *context);
void tunuino_usart2_send_byte(uint8_t byte, void *context);
#endif
