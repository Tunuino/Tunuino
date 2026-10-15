// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "mcu_tick.h"
#define REG32(addr) (*(volatile uint32_t *)(addr))
#define SYST_CSR REG32(0xE000E010u)
#define SYST_RVR REG32(0xE000E014u)
#define SYST_CVR REG32(0xE000E018u)
static volatile uint32_t elapsed_ms;
void tunuino_tick_init(void) {
    elapsed_ms = 0u;
    SYST_CSR = 0u;
    SYST_RVR = 16000u - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 7u; // processor clock, interrupt, enable
}
uint32_t tunuino_millis(void) { return elapsed_ms; }
void tunuino_tick_irq(void) { ++elapsed_ms; }
void SysTick_Handler(void) { tunuino_tick_irq(); }
