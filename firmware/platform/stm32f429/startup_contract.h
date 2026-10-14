// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_STARTUP_CONTRACT_H
#define TUNUINO_STARTUP_CONTRACT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    TUNUINO_STARTUP_RESET = 0,
    TUNUINO_STARTUP_OUTPUTS_SAFE,
    TUNUINO_STARTUP_CLOCKS_READY,
    TUNUINO_STARTUP_PERIPHERALS_READY,
    TUNUINO_STARTUP_FAULT
} tunuino_startup_phase_t;
typedef struct {
    tunuino_startup_phase_t phase;
    uint8_t hardware_output_enable;
} tunuino_startup_t;
void tunuino_startup_init(tunuino_startup_t *startup);
uint8_t tunuino_startup_advance(tunuino_startup_t *startup, tunuino_startup_phase_t next);
void tunuino_startup_fault(tunuino_startup_t *startup);
#ifdef __cplusplus
}
#endif
#endif
