// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "usart_console.h"
#define REG32(address) (*(volatile uint32_t *)(address))
#define RCC_APB1ENR REG32(0x40023840u)
#define USART3_SR REG32(0x40004800u)
#define USART3_DR REG32(0x40004804u)
#define USART3_BRR REG32(0x40004808u)
#define USART3_CR1 REG32(0x4000480Cu)
#define USART3_CR2 REG32(0x40004810u)
#define USART3_CR3 REG32(0x40004814u)
tunuino_usart_config_t tunuino_usart_make_config(uint32_t pclk_hz, uint32_t baud) {
    tunuino_usart_config_t cfg = {0};
    cfg.peripheral_clock_hz = pclk_hz;
    cfg.baud_rate = baud;
    if (!pclk_hz || !baud || baud > pclk_hz / 16u) return cfg;
    // Oversampling by 16, rounded to nearest integer.
    const uint32_t divisor = ((uint64_t)pclk_hz + baud / 2u) / baud;
    if (divisor < 16u || divisor > 0xffffu) return cfg;
    cfg.brr = (uint16_t)divisor;
    cfg.valid = 1u;
    return cfg;
}
uint8_t tunuino_usart3_init_disconnected(const tunuino_usart_config_t *cfg) {
    if (!cfg || !cfg->valid) return 0u;
    RCC_APB1ENR |= (1u << 18); // USART3EN
    (void)RCC_APB1ENR;
    USART3_CR1 = 0u; // UE=0: peripheral disabled
    USART3_CR2 = 0u;
    USART3_CR3 = 0u;
    USART3_BRR = cfg->brr;
    // Do not set UE, TE, RE or any IRQ bits before pinout is verified.
    return 1u;
}
uint8_t tunuino_usart3_tx_ready(void *context) {
    (void)context;
    // No transmission unless both UE and TE are enabled by a future reviewed driver.
    if ((USART3_CR1 & ((1u << 13) | (1u << 3))) != ((1u << 13) | (1u << 3)))
        return 0u;
    return (uint8_t)((USART3_SR & (1u << 7)) != 0u);
}
void tunuino_usart3_send_byte(uint8_t byte, void *context) {
    (void)context;
    if (tunuino_usart3_tx_ready(0)) USART3_DR = byte;
}
