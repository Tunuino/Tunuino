/* Copyright (c) 2026 Tunuino. */
#ifndef TUNUINO_PROTEUS_BENCH_BOARD_H
#define TUNUINO_PROTEUS_BENCH_BOARD_H
#include <stdint.h>
void board_init(void);
void board_usb_vbus_init(void);
uint64_t board_millis(void);
void board_uid(uint8_t uid[12]);
extern volatile uint32_t tunuino_bench_fault;
#endif
