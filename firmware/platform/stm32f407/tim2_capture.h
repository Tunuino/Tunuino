// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TIM2_CAPTURE_H
#define TUNUINO_TIM2_CAPTURE_H
#include <stdint.h>
// TIM2 is a 32-bit counter on STM32F407. This driver is deliberately disarmed.
// No GPIO alternate function, input capture channel or NVIC IRQ is enabled.
typedef struct {
    uint32_t timer_clock_hz;
    uint16_t prescaler;
    uint8_t valid;
} tunuino_tim2_clock_config_t;
tunuino_tim2_clock_config_t tunuino_tim2_make_1mhz_config(uint32_t timer_clock_hz);
// Initializes free-running TIM2 counter only; does not connect sensor inputs.
// Caller must verify actual TIM2 peripheral clock before calling.
uint8_t tunuino_tim2_init_counter(const tunuino_tim2_clock_config_t *config);
uint32_t tunuino_tim2_counter(void);
#endif
