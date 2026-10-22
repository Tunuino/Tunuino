/* Copyright (c) 2026 Tunuino. No engine GPIO is configured. */
#include "ch.h"
#include "hal.h"
#include "board.h"

_Static_assert(STM32_SYSCLK == 168000000, "Unexpected Proteus system clock");
_Static_assert(STM32_PLL48CLK == 48000000, "USB requires 48 MHz");

volatile uint32_t tunuino_bench_fault;

/* ChibiOS startup calls this before C runtime initialization. */
void __early_init(void) {
  stm32_clock_init();
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  (void)RCC->AHB1ENR;
}

void boardInit(void) {
  palSetPadMode(GPIOA, 11, PAL_MODE_ALTERNATE(10));
  palSetPadMode(GPIOA, 12, PAL_MODE_ALTERNATE(10));
}

void board_uid(uint8_t uid[12]) {
  const volatile uint8_t *source = (const volatile uint8_t *)UID_BASE;
  for (unsigned i = 0; i < 12; ++i) uid[i] = source[i];
}

uint64_t board_millis(void) {
  static uint32_t previous;
  static uint64_t epoch;
  uint32_t now = (uint32_t)chVTGetSystemTimeX();
  _Static_assert(CH_CFG_ST_FREQUENCY == 1000, "Bench clock is milliseconds");
  if (now < previous) epoch += UINT64_C(1) << 32;
  previous = now;
  return epoch | now;
}
