/* Copyright (c) 2026 Tunuino. Experimental USB-only bench firmware.
 * No acquisition, calibration writes, engine outputs or flash-writing code. */
#include "board.h"
#include "tusb.h"
#include "tunuino/link.h"

static tunuino_link link;
static tunuino_identity identity = {TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,0,1,0,{0}};
static const tunuino_dispatch_context context = {&identity,NULL,NULL,NULL,NULL};
static bool allow_session, active, completed;
static uint32_t pending;
static uint8_t receive[64];
static size_t received, offset;

void tud_mount_cb(void) {
    allow_session = true; active = false; pending = 0; completed = false;
    received = offset = 0;
}
void tud_umount_cb(void) {
    tunuino_link_abort(&link);
    active = allow_session = false; pending = 0; completed = false;
    received = offset = 0;
}
void tud_cdc_line_state_cb(uint8_t interface, bool dtr, bool rts) {
    (void)interface; (void)rts;
    if (!dtr && active) {
        tunuino_link_abort(&link);
        active = allow_session = false;
        received = offset = 0;
    }
}
void tud_cdc_tx_complete_cb(uint8_t interface) {
    (void)interface;
    if (pending != 0) completed = true;
}
static void stop_session(void) {
    tunuino_link_abort(&link);
    active = allow_session = false;
    tunuino_bench_fault = 20;
}
int main(void) {
    board_init();
    board_uid(identity.uid);
    if (tunuino_identity_validate(&identity) != TUNUINO_IDENTITY_OK) {
        tunuino_bench_fault = 10;
        for (;;) {}
    }
    const tusb_rhport_init_t port = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL};
    if (!tusb_init(0,&port)) {
        tunuino_bench_fault = 11;
        for (;;) {}
    }
    for (;;) {
        tud_task();
        uint64_t now = board_millis();
        if (!tud_cdc_connected()) continue;
        if (!active && allow_session) {
            /* First request may already be queued with the DTR event. */
            if (tunuino_link_init(&link,&context,1000,1000,now) != TUNUINO_LINK_RECEIVING) {
                stop_session(); continue;
            }
            active = true;
        }
        if (!active) continue;
        tunuino_link_status state = tunuino_link_poll(&link,now);
        if (state != TUNUINO_LINK_RECEIVING && state != TUNUINO_LINK_RESPONDING) {
            stop_session(); continue;
        }
        if (completed) {
            completed = false;
            state = tunuino_link_tx_advance(&link,pending,now);
            pending = 0;
            if (state != TUNUINO_LINK_RECEIVING && state != TUNUINO_LINK_RESPONDING) {
                stop_session(); continue;
            }
        }
        if (state == TUNUINO_LINK_RECEIVING) {
            if (offset == received) {
                received = tud_cdc_read(receive,sizeof(receive)); offset = 0;
            }
            if (offset < received) {
                size_t consumed;
                state = tunuino_link_feed(&link,receive + offset,received - offset,now,&consumed);
                offset += consumed;
                if (state != TUNUINO_LINK_RECEIVING && state != TUNUINO_LINK_RESPONDING) {
                    stop_session(); continue;
                }
            }
        }
        if (state == TUNUINO_LINK_RESPONDING && pending == 0) {
            const uint8_t *bytes; size_t remaining;
            if (tunuino_link_tx_view(&link,now,&bytes,&remaining) != TUNUINO_LINK_RESPONDING) {
                stop_session(); continue;
            }
            /* Single short packet: no TinyUSB automatic ZLP callback ambiguity.
             * Advance only after endpoint completion, never at FIFO acceptance. */
            if (remaining > 63) remaining = 63;
            pending = tud_cdc_write(bytes,(uint32_t)remaining);
            if (pending != 0) tud_cdc_write_flush();
        }
    }
}
