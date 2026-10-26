/* Real board LED tasks driven by host GPIO and interrupt-timer models. */
#include <stdio.h>
#include "board.h"
#include "board_status.h"
#include "chibios_status_model.h"
volatile uint32_t tunuino_bench_fault;
static bool on(unsigned pin) { return (status_high & (1u << pin)) != 0; }
static void reset(systime_t time) {
    status_now = time; status_high = status_outputs = 0;
    tunuino_bench_fault = 0;
    board_status_start();
    assert(status_outputs == (15u << 3) && status_high == 0);
}
static void boot(void) {
    status_advance(100); assert(status_high == (15u << 3));
    status_advance(100); assert(status_high == 0);
    status_advance(100); assert(on(4) && !on(3) && !on(5) && !on(6));
}
int main(void) {
    tunuino_device device = {0};
    reset(0); boot();
    /* No USB: 33ms on / 19ms off; USB ready: 99ms on / 59ms off. */
    status_advance(32); assert(on(4)); status_advance(1); assert(!on(4));
    status_advance(18); assert(!on(4));
    board_status_usb(true); status_advance(1); assert(on(4));
    status_advance(98); assert(on(4)); status_advance(1); assert(!on(4));
    status_advance(58); assert(!on(4));
    board_status_activity(); status_advance(1); assert(on(4));
    /* Valid exchange: 33ms on / 100ms off. */
    status_advance(32); assert(on(4)); status_advance(1); assert(!on(4));
    status_advance(99); assert(!on(4)); status_advance(1); assert(on(4));
    /* USB detach clears activity; reconnect is not a valid exchange. */
    board_status_usb(false); board_status_usb(true);
    status_advance(33); assert(!on(4)); status_advance(59); assert(on(4));
    status_advance(98); assert(on(4)); status_advance(1); assert(!on(4));
    board_status_flash(true); status_advance(500); assert(on(4));
    status_advance(58); assert(on(4)); status_advance(1); assert(!on(4));
    status_advance(499); assert(!on(4)); status_advance(1); assert(on(4));
    board_status_flash(false);

    /* Staged settings keep the pending-save rhythm without a flash call. */
    reset(0); boot(); device.dirty = true; board_status_inputs(&device);
    status_advance(33); assert(!on(4));
    status_advance(499); assert(!on(4)); status_advance(1); assert(on(4));
    status_advance(499); assert(on(4)); status_advance(1); assert(!on(4));
    device.dirty = false; board_status_inputs(&device);

    /* Engine LED: stopped off, cranking steady, running toggles every 50ms. */
    device.live.valid_mask = TUNUINO_LIVE_RPM_VALID; device.live.rpm_milli = 200000;
    board_status_inputs(&device); status_advance(50); assert(on(5));
    device.live.rpm_milli = 1000000; board_status_inputs(&device);
    status_advance(50); assert(!on(5)); status_advance(50); assert(on(5));
    device.live.valid_mask = 0; board_status_inputs(&device);
    status_advance(50); assert(!on(5));

    /* Critical failure overrides USB/activity/flash; error stays lit. */
    reset(0); tunuino_bench_fault = 11;
    board_status_usb(true); board_status_activity(); board_status_flash(true);
    status_advance(100); assert(on(3) && on(4));
    status_advance(449); assert(on(3) && on(4));
    status_advance(1); assert(on(3) && !on(4));
    status_advance(49); assert(!on(4)); status_advance(1); assert(on(4));
    assert(!on(5));

    /* Recoverable USB interruption is a warning, not a critical error. */
    reset(0); boot(); tunuino_bench_fault = 20;
    status_advance(33); assert(!on(3) && on(6));
    tunuino_bench_fault = 0;
    device.trigger_errors = 1; device.active[2] = 1;
    board_status_inputs(&device); status_advance(100); assert(on(6));
    status_advance(600); board_status_inputs(&device); status_advance(100); assert(on(6));
    device.trigger_valid = true; board_status_inputs(&device);
    status_advance(100); assert(!on(6));

    /* Timer and session-age arithmetic survive the 32-bit tick wrap. */
    reset(UINT32_MAX - 400u); boot(); board_status_usb(true); board_status_activity();
    status_advance(33); assert(!on(4)); status_advance(100); assert(on(4));
    status_advance(1100);
    /* Next full USB-ready cycle has 99ms on, rather than session timing. */
    while(on(4)) status_advance(1);
    while(!on(4)) status_advance(1);
    status_advance(98); assert(on(4)); status_advance(1); assert(!on(4));
    chSysLock(); board_status_halt(); assert(on(3)); chSysUnlock();
    puts("Proteus LEDs: boot, exact communication timings, engine states, warnings, fatal/halt and tick wrap passed. Host GPIO/timers; no physical ECU.");
    return 0;
}
