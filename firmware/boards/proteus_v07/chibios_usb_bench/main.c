/* Copyright (c) 2026 Tunuino. ChibiOS input/calibration bench; no engine outputs. */
#include "ch.h"
#include "hal.h"
#include "board.h"
#include "usbcfg.h"
#include "tunuino/link.h"
#include "tunuino/bench_tunerstudio.h"
#include "board_io.h"

static tunuino_link link;
static tunuino_device device;
static tunuino_identity identity = {TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,
    0,1,0,TUNUINO_IO_CAPABILITIES,{0}};
static const tunuino_dispatch_context context = {&identity,&device.live,&device.ve,&device.ignition,&device};

int main(void) {
  event_listener_t events;
  uint32_t generation = 0;
  size_t received = 0, offset = 0, pending = 0;
  uint8_t receive[64];
  bool active = false;
  halInit();
  chSysInit();
  tunuino_device_init(&device,board_io_save,board_io_clock,NULL);
  board_io_start(&device);
  board_uid(identity.uid);
  if (tunuino_identity_validate(&identity) != TUNUINO_IDENTITY_OK) {
    tunuino_bench_fault = 10;
    for (;;) chThdSleepMilliseconds(100);
  }
  tunuino_usb_serial_init();
  sduObjectInit(&SDU1);
  if (sduStart(&SDU1, &serusbcfg) != MSG_OK) {
    tunuino_bench_fault = 11;
    for (;;) chThdSleepMilliseconds(100);
  }
  chEvtRegisterMaskWithFlags(chnGetEventSource(&SDU1), &events, EVENT_MASK(0),
                           CHN_TRANSMISSION_END | CHN_DISCONNECTED);
  /* Detach before starting the controller so the host sees a fresh device. */
  usbDisconnectBus(&USBD1);
  chThdSleepMilliseconds(250);
  if (usbStart(&USBD1, &usbcfg) != MSG_OK) {
    tunuino_bench_fault = 12;
    for (;;) chThdSleepMilliseconds(100);
  }
  usbConnectBus(&USBD1);
  board_io_resync(&device);

  for (;;) {
    board_io_update(&device);
    uint64_t now;
    uint32_t live_generation;
    bool configured;
    eventflags_t flags = chEvtGetAndClearFlags(&events);
    chSysLock();
    live_generation = tunuino_usb_generation;
    configured = usbGetDriverStateI(&USBD1) == USB_ACTIVE;
    chSysUnlock();
    now = board_millis();
    if (!configured || live_generation != generation) {
      if (active) goto recover;
      tunuino_link_abort(&link);
      active = false; pending = received = offset = 0;
      generation = live_generation;
      if (!configured) { chThdSleepMilliseconds(1); continue; }
    }
    if (active) {
      tunuino_link_status state = tunuino_link_poll(&link, now);
      if (state != TUNUINO_LINK_RECEIVING && state != TUNUINO_LINK_RESPONDING) goto recover;
      if (pending && (flags & CHN_TRANSMISSION_END)) {
        state = tunuino_link_tx_advance(&link, pending, now);
        pending = 0;
        if (state != TUNUINO_LINK_RECEIVING && state != TUNUINO_LINK_RESPONDING) goto recover;
      }
    }
    if (pending == 0 && (!active || tunuino_link_poll(&link, now) == TUNUINO_LINK_RECEIVING)) {
      if (offset == received) {
        received = chnReadTimeout(&SDU1, receive, sizeof(receive), TIME_MS2I(1));
        offset = 0;
      }
      now = board_millis();
      if (offset < received) {
        size_t consumed;
        if (!active) {
          if (tunuino_link_init(&link, &context, 1000, 1000, now) != TUNUINO_LINK_RECEIVING) goto recover;
          active = true;
        }
        tunuino_link_status state = tunuino_link_feed(&link, receive + offset,
            received - offset, now, &consumed);
        offset += consumed;
        if (state != TUNUINO_LINK_RECEIVING && state != TUNUINO_LINK_RESPONDING) goto recover;
      }
    }
    if (active && !pending && tunuino_link_poll(&link, now) == TUNUINO_LINK_RESPONDING) {
      const uint8_t *bytes;
      size_t remaining;
      if (tunuino_link_tx_view(&link, now, &bytes, &remaining) != TUNUINO_LINK_RESPONDING) goto recover;
      if (remaining > 63) remaining = 63;
      (void)chEvtGetAndClearFlags(&events);
      pending = chnWriteTimeout(&SDU1, bytes, remaining, TIME_MS2I(1));
      /* Queue acceptance is not completion. Advance only on the physical
       * CHN_TRANSMISSION_END notification from ChibiOS sduDataTransmitted. */
    }
    chThdSleepMilliseconds(1);
    continue;

recover:
    /* Cancel hardware and reset both CDC queues before reusing link storage.
     * A fresh USB enumeration separates old bytes/completion flags from the
     * next connection. Acquisition and saved calibration survive this reset. */
    usbDisconnectBus(&USBD1);usbStop(&USBD1);sduStop(&SDU1);
    tunuino_link_abort(&link);active=false;pending=received=offset=0;generation=0;
    tunuino_bench_fault=20;
    chThdSleepMilliseconds(250);
    if(sduStart(&SDU1,&serusbcfg)!=MSG_OK || usbStart(&USBD1,&usbcfg)!=MSG_OK)goto recover;
    (void)chEvtGetAndClearFlags(&events);
    usbConnectBus(&USBD1);board_io_resync(&device);tunuino_bench_fault=0;
  }
}
