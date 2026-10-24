/* Host register-model checks; do not establish physical USB enumeration. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "board.h"

USB_OTG_GlobalTypeDef tunuino_test_usb;

static void check(uint32_t before) {
    tunuino_test_usb.GCCFG = before;
    board_usb_vbus_init();
    uint32_t after = tunuino_test_usb.GCCFG;
    /* STM32F427: disable A/B external sensing and keep a valid device session. */
    assert((after & (UINT32_C(1) << 21)) != 0);
    assert((after & ((UINT32_C(1) << 18) | (UINT32_C(1) << 19))) == 0);
    uint32_t unrelated = ~((UINT32_C(1) << 18) | (UINT32_C(1) << 19) | (UINT32_C(1) << 21));
    assert((after & unrelated) == (before & unrelated));
    board_usb_vbus_init();
    assert(tunuino_test_usb.GCCFG == after);
}

int main(void) {
    check(0); /* Peripheral reset state: original setup left the session invalid. */
    check(UINT32_MAX);
    for (unsigned bit = 0; bit < 32; ++bit) {
        check(UINT32_C(1) << bit);
        check(~(UINT32_C(1) << bit));
    }
    puts("F427 USB VBUS register-model checks passed: reset state, sensing disabled, unrelated bits preserved, repeat initialization. No physical USB test.");
    return 0;
}
