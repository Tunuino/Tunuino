// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "mcu_clock.h"
#define REG32(addr) (*(volatile uint32_t *)(addr))
#define RCC_CR REG32(0x40023800u)
#define RCC_CFGR REG32(0x40023808u)
#define FLASH_ACR REG32(0x40023C00u)
uint8_t tunuino_clock_init_hsi(void) {
    RCC_CR |= 1u; // HSION
    uint32_t timeout = 1000000u;
    while (!(RCC_CR & (1u << 1)) && timeout) --timeout; // HSIRDY
    if (!timeout) return 0u;
    RCC_CFGR &= ~((3u << 0) | (15u << 4) | (7u << 10) | (7u << 13));
    // Reset APB/AHB prescalers to /1, select HSI as SYSCLK.
    timeout = 1000000u;
    while (((RCC_CFGR >> 2) & 3u) != 0u && timeout) --timeout;
    if (!timeout) return 0u;
    FLASH_ACR &= ~7u; // 0 wait states at 16 MHz (voltage requirements apply).
    return 1u;
}
