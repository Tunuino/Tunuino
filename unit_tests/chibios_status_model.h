/* Host-only LED pins and interrupt timers. Never linked into the ECU. */
#ifndef TUNUINO_CHIBIOS_STATUS_MODEL_H
#define TUNUINO_CHIBIOS_STATUS_MODEL_H
#include <assert.h>
#include <string.h>
#include "hal.h"
static systime_t status_now;
static bool status_locked;
static uint32_t status_high, status_outputs;
static virtual_timer_t *status_timers[2];

void chSysLock(void) { assert(!status_locked); status_locked = true; }
void chSysUnlock(void) { assert(status_locked); status_locked = false; }
void chSysLockFromISR(void) { chSysLock(); }
void chSysUnlockFromISR(void) { chSysUnlock(); }
systime_t chVTGetSystemTimeX(void) { return status_now; }
void chVTObjectInit(virtual_timer_t *timer) {
    memset(timer, 0, sizeof(*timer));
    for (unsigned i = 0; i < 2; ++i) {
        if (status_timers[i] == NULL || status_timers[i] == timer) {
            status_timers[i] = timer;
            return;
        }
    }
    assert(0);
}
void chVTSetI(virtual_timer_t *timer, sysinterval_t delay, vtfunc_t callback, void *argument) {
    assert(status_locked && delay > 0);
    timer->armed = true; timer->due = status_now + delay;
    timer->callback = callback; timer->argument = argument;
}
void chVTSet(virtual_timer_t *timer, sysinterval_t delay, vtfunc_t callback, void *argument) {
    chSysLock(); chVTSetI(timer, delay, callback, argument); chSysUnlock();
}
void palSetPad(void *port, unsigned pad) {
    assert(port == GPIOE && pad >= 3 && pad <= 6);
    status_high |= 1u << pad;
}
void palClearPad(void *port, unsigned pad) {
    assert(port == GPIOE && pad >= 3 && pad <= 6);
    status_high &= ~(1u << pad);
}
void palSetPadMode(void *port, unsigned pad, unsigned mode) {
    assert(port == GPIOE && pad >= 3 && pad <= 6 && mode == PAL_MODE_OUTPUT_PUSHPULL);
    /* LED latches must be inactive before changing the pin mode at startup.
     * The terminal halt path explicitly sets its error latch first. */
    status_outputs |= 1u << pad;
}
static void status_advance(unsigned milliseconds) {
    assert(!status_locked);
    for (unsigned elapsed = 0; elapsed < milliseconds; ++elapsed) {
        ++status_now;
        for (unsigned i = 0; i < 2; ++i) {
            virtual_timer_t *timer = status_timers[i];
            if (timer != NULL && timer->armed && (int32_t)(status_now - timer->due) >= 0) {
                timer->armed = false;
                timer->callback(timer, timer->argument);
                assert(!status_locked);
            }
        }
    }
}
#endif
