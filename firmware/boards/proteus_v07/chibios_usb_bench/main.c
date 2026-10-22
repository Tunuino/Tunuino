/* Copyright (c) 2026 Tunuino. ChibiOS USB-only bench; no engine outputs. */
#include "ch.h"
#include "hal.h"
#include "board.h"
#include "usbcfg.h"
#include "tunuino/link.h"

static tunuino_link link;
static tunuino_identity identity = {TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,0,2,0,{0}};
static const tunuino_dispatch_context context = {&identity,NULL,NULL,NULL};

static void stop_session(void) {
  /* Stop the controller before invalidating any outstanding TX snapshot. */
  usbDisconnectBus(&USBD1);
  usbStop(&USBD1);
  tunuino_link_abort(&link);
  tunuino_bench_fault = 20;
  for (;;) chThdSleepMilliseconds(100);
}

int main(void) {
  event_listener_t events;
  uint32_t generation = 0;
  size_t received = 0, offset = 0, pending = 0;
  uint8_t receive[64];
  bool active = false;
  halInit();
  chSysInit();
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
  /* Same detach/start/connect lifecycle as rusEFI usb_serial_start(). */
  usbDisconnectBus(&USBD1);
  chThdSleepMilliseconds(250);
  if (usbStart(&USBD1, &usbcfg) != MSG_OK) {
    tunuino_bench_fault = 12;
    for (;;) chThdSleepMilliseconds(100);
  }
  usbConnectBus(&USBD1);

  for (;;) {
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
      if (active) stop_session();
      tunuino_link_abort(&link);
      active = false; pending = received = offset = 0;
      generation = live_generation;
      if (!configured) { chThdSleepMilliseconds(1); continue; }
    }
    if (active) {
      tunuino_link_status state = tunuino_link_poll(&link, now);
      if (state != TUNUINO_LINK_RECEIVING && state != TUNUINO_LINK_RESPONDING) stop_session();
      if (pending && (flags & CHN_TRANSMISSION_END)) {
        state = tunuino_link_tx_advance(&link, pending, now);
        pending = 0;
        if (state != TUNUINO_LINK_RECEIVING && state != TUNUINO_LINK_RESPONDING) stop_session();
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
          if (tunuino_link_init(&link, &context, 1000, 1000, now) != TUNUINO_LINK_RECEIVING) stop_session();
          active = true;
        }
        tunuino_link_status state = tunuino_link_feed(&link, receive + offset,
            received - offset, now, &consumed);
        offset += consumed;
        if (state != TUNUINO_LINK_RECEIVING && state != TUNUINO_LINK_RESPONDING) stop_session();
      }
    }
    if (active && !pending && tunuino_link_poll(&link, now) == TUNUINO_LINK_RESPONDING) {
      const uint8_t *bytes;
      size_t remaining;
      if (tunuino_link_tx_view(&link, now, &bytes, &remaining) != TUNUINO_LINK_RESPONDING) stop_session();
      if (remaining > 63) remaining = 63;
      (void)chEvtGetAndClearFlags(&events);
      pending = chnWriteTimeout(&SDU1, bytes, remaining, TIME_MS2I(1));
      /* Queue acceptance is not completion. Advance only on the physical
       * CHN_TRANSMISSION_END notification from ChibiOS sduDataTransmitted. */
    }
    chThdSleepMilliseconds(1);
  }
}
