/* Copyright (c) 2026 Tunuino. STM32F427 native USB FS device-only setup. */
#include "stm32f4xx.h"
#include "board.h"

void board_usb_vbus_init(void) {
    /* Proteus does not use PA9 as VBUS sense. The F427 uses GCCFG here,
     * unlike newer STM32 cores with GOTGCTL B-session-valid override bits.
     * Match ST's USB_DevInit(vbus_sensing_enable = 0) for the F427 and
     * rusEFI's BOARD_OTG_NOVBUSSENS. Call after peripheral reset, before
     * TinyUSB initializes the PHY and connects its D+ pull-up.
     * No GPIO other than the USB data pins is needed or configured. */
    USB_OTG_FS->GCCFG = (USB_OTG_FS->GCCFG
        & ~(USB_OTG_GCCFG_VBUSASEN | USB_OTG_GCCFG_VBUSBSEN))
        | USB_OTG_GCCFG_NOVBUSSENS;
}
