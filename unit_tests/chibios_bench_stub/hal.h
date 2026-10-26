/* Host-only USB/SerialUSB interface model. No hardware registers. */
#ifndef TUNUINO_TEST_HAL_H
#define TUNUINO_TEST_HAL_H
#include <stddef.h>
#include <stdint.h>
#include "ch.h"
typedef struct { int state; } USBDriver;
typedef struct { unsigned unused; } USBConfig;
typedef struct { USBDriver *usbp; unsigned bulk_in, bulk_out, int_in; } SerialUSBConfig;
typedef struct { unsigned event; } SerialUSBDriver;
extern USBDriver USBD1;
#define USB_ACTIVE 4
#define GPIOE ((void *)5)
#define PAL_MODE_OUTPUT_PUSHPULL 1u
void palSetPad(void *port, unsigned pad);
void palClearPad(void *port, unsigned pad);
void palSetPadMode(void *port, unsigned pad, unsigned mode);
#define CHN_TRANSMISSION_END 16u
#define CHN_DISCONNECTED 2u
#define chnGetEventSource(p) (&(p)->event)
#define usbGetDriverStateI(p) ((p)->state)
void halInit(void);
void sduObjectInit(SerialUSBDriver *sdu);
int sduStart(SerialUSBDriver *sdu, const SerialUSBConfig *config);
void sduStop(SerialUSBDriver *sdu);
int usbStart(USBDriver *usb, const USBConfig *config);
void usbStop(USBDriver *usb);
void usbDisconnectBus(USBDriver *usb);
void usbConnectBus(USBDriver *usb);
size_t chnReadTimeout(SerialUSBDriver *sdu, uint8_t *bytes, size_t count, uint32_t timeout);
size_t chnWriteTimeout(SerialUSBDriver *sdu, const uint8_t *bytes, size_t count, uint32_t timeout);
#endif
