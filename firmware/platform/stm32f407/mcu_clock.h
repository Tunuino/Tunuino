// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_MCU_CLOCK_H
#define TUNUINO_MCU_CLOCK_H
#include <stdint.h>
#define TUNUINO_HSI_HZ 16000000u
// Explicitly select the internal HSI oscillator, no PLL or external crystal.
uint8_t tunuino_clock_init_hsi(void);
#endif
