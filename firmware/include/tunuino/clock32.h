/* Copyright (c) 2026 Tunuino. */
#ifndef TUNUINO_CLOCK32_H
#define TUNUINO_CLOCK32_H
#include <stdint.h>
typedef enum {
    TUNUINO_CLOCK32_OK = 0, TUNUINO_CLOCK32_FAULT, TUNUINO_CLOCK32_ARGUMENT
} tunuino_clock32_status;
/* Private fields. One serialized caller; zero initialization is safe but
 * init is required. Not an atomic clock shared between tasks/interrupts. */
typedef struct {
    uint64_t time_us;
    uint32_t previous, max_step_us;
    unsigned initialized, fault;
} tunuino_clock32;
/* Requires a real free-running 32-bit counter at exactly 1 MHz.
 * first_us is the software time corresponding to first_counter.
 * max_step_us: 1..INT32_MAX, inclusive. The driver MUST guarantee that the
 * real elapsed time between observations never exceeds this bound, even
 * without engine edges. Missing a whole wrap cannot be detected from raw
 * counter values. Reset/reconfiguration/suspend requires quiescent re-init.
 * Do not mix current-counter samples with older capture-register values. */
tunuino_clock32_status tunuino_clock32_init(tunuino_clock32 *clock,
    uint32_t first_counter, uint64_t first_us, uint32_t max_step_us);
/* Extends a chronologically observed counter. Allows identical ticks;
 * unexpected deltas and uint64 overflow latch FAULT until re-init.
 * Errors zero output; output must not overlap clock storage. Caller must
 * invalidate its acquisition pipeline on FAULT; no other object is changed. */
tunuino_clock32_status tunuino_clock32_observe(tunuino_clock32 *clock,
    uint32_t counter, uint64_t *time_us);
#endif
