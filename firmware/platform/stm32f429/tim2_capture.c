// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "tim2_capture.h"
#include <stddef.h>
#define REG32(addr) (*(volatile uint32_t *)(addr))
#define RCC_APB1ENR REG32(0x40023840u)
#define TIM2_CR1 REG32(0x40000000u)
#define TIM2_DIER REG32(0x4000000Cu)
#define TIM2_SR REG32(0x40000010u)
#define TIM2_CCMR1 REG32(0x40000018u)
#define TIM2_CCER REG32(0x40000020u)
#define TIM2_CNT REG32(0x40000024u)
#define TIM2_PSC REG32(0x40000028u)
#define TIM2_ARR REG32(0x4000002Cu)
#define TIM2_EGR REG32(0x40000014u)
tunuino_tim2_clock_config_t tunuino_tim2_make_1mhz_config(uint32_t hz) {
    tunuino_tim2_clock_config_t cfg = {0};
    cfg.timer_clock_hz = hz;
    if (hz >= 1000000u && hz % 1000000u == 0u &&
        hz / 1000000u <= 65536u) {
        cfg.prescaler = (uint16_t)(hz / 1000000u - 1u);
        cfg.valid = 1u;
    }
    return cfg;
}
uint8_t tunuino_tim2_init_counter(const tunuino_tim2_clock_config_t *cfg) {
    if (!cfg || !cfg->valid) return 0u;
    RCC_APB1ENR |= 1u; // TIM2EN
    (void)RCC_APB1ENR; // allow clock enable to propagate
    TIM2_CR1 = 0u;
    TIM2_DIER = 0u; // all timer interrupts/DMA disabled
    TIM2_CCER = 0u; // all capture/compare channels disabled
    TIM2_CCMR1 = 0u;
    TIM2_PSC = cfg->prescaler;
    TIM2_ARR = 0xFFFFFFFFu;
    TIM2_EGR = 1u; // load prescaler
    TIM2_SR = 0u;
    TIM2_CNT = 0u;
    TIM2_CR1 = 1u; // counter enabled, no input channels
    return 1u;
}
uint32_t tunuino_tim2_counter(void) { return TIM2_CNT; }
