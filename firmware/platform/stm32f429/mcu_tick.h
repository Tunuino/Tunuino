// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_MCU_TICK_H
#define TUNUINO_MCU_TICK_H
#include <stdint.h>
// 1 ms tick based on 16 MHz HSI; not a precision engine event scheduler.
void tunuino_tick_init(void);
uint32_t tunuino_millis(void);
void tunuino_tick_irq(void);
#endif
