// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_USART_CONSOLE_H
#define TUNUINO_USART_CONSOLE_H
#include <stdint.h>
// STM32F407 USART2 on PD5 (TX), PD6 (RX), AF7, as rusEFI Proteus F4 reference.\n// APB1 clock; no GPIO, NVIC or UART enable performed.
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
