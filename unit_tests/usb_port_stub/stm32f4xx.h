/* Register model for host tests only, not a hardware driver. */
#ifndef TUNUINO_TEST_USB_STM32F4XX_H
#define TUNUINO_TEST_USB_STM32F4XX_H
#include <stdint.h>
typedef struct { volatile uint32_t GCCFG; } USB_OTG_GlobalTypeDef;
extern USB_OTG_GlobalTypeDef tunuino_test_usb;
#define USB_OTG_FS (&tunuino_test_usb)
#define USB_OTG_GCCFG_VBUSASEN (UINT32_C(1) << 18)
#define USB_OTG_GCCFG_VBUSBSEN (UINT32_C(1) << 19)
#define USB_OTG_GCCFG_NOVBUSSENS (UINT32_C(1) << 21)
#endif
