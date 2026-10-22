/* Copyright (c) 2026 Tunuino. */
#include "tunuino/link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tunuino_link link;
static tunuino_identity identity = {TUNUINO_PROTEUS,TUNUINO_STM32_F4,1,0,1,0,3,{1,2,3,4,5,6,7,8,9,10,11,12}};
static tunuino_live_sample live = {1234,950000,100000,1000000,-12345,15};
static tunuino_map map;
static tunuino_map_snapshot ve, ignition;
static tunuino_dispatch_context context = {&identity,&live,&ve,&ignition};
static uint8_t request[64], wire[TUNUINO_MAX_BODY + TUNUINO_FRAME_OVERHEAD], file[TUNUINO_MAP_FILE_MAX];
static size_t request_length;
static void init(uint32_t rx_ms, uint32_t tx_ms, uint64_t now) {
    assert(tunuino_link_init(&link, &context, rx_ms, tx_ms, now) == TUNUINO_LINK_RECEIVING);
}
static void query(const uint8_t *body, size_t length) {
    assert(tunuino_frame_encode(body, length, request, sizeof(request), &request_length) == TUNUINO_FRAME_OK);
}
static void text_query(const char *body) { query((const uint8_t *)body, strlen(body)); }
static const uint8_t *drain(uint64_t now, size_t *body_length) {
    const uint8_t *view; size_t remaining, consumed;
    assert(tunuino_link_feed(&link, request, request_length, now, &consumed) == TUNUINO_LINK_RESPONDING && consumed == 0);
    assert(tunuino_link_tx_advance(&link, 0, now) == TUNUINO_LINK_RESPONDING);
    size_t sent = 0;
    do {
        assert(tunuino_link_tx_view(&link, now, &view, &remaining) == TUNUINO_LINK_RESPONDING);
        assert(remaining > 0 && sent + remaining <= sizeof(wire));
        size_t count = remaining > 3 ? 3 : remaining;
        memcpy(wire + sent, view, count); sent += count;
        assert(tunuino_link_tx_advance(&link, count, now) == (count == remaining ? TUNUINO_LINK_RECEIVING : TUNUINO_LINK_RESPONDING));
    } while (link.state == TUNUINO_LINK_RESPONDING);
    assert(tunuino_link_tx_view(&link, now, &view, &remaining) == TUNUINO_LINK_RECEIVING && view == NULL && remaining == 0);
    const uint8_t *body;
    assert(tunuino_frame_decode(wire, sent, &body, body_length) == TUNUINO_FRAME_OK);
    return body;
}
static void sticky(tunuino_link_status expected, uint64_t now) {
    size_t consumed = 123, remaining = 123;
    const uint8_t *view = request;
    assert(tunuino_link_poll(&link, now) == expected);
    assert(tunuino_link_feed(&link, request, request_length, now, &consumed) == expected && consumed == 0);
    assert(tunuino_link_tx_view(&link, now, &view, &remaining) == expected && view == NULL && remaining == 0);
    assert(tunuino_link_tx_advance(&link, 1, now) == expected);
}
static void mailbox_checks(void) {
    static tunuino_live_mailbox mailbox;
    tunuino_live_mailbox unconfigured = {0};
    assert(tunuino_live_mailbox_init(&mailbox) == TUNUINO_MAILBOX_READY);
    tunuino_dispatch_context providers = context;
    providers.live = NULL; /* The mailbox supplies the live provider. */
    assert(tunuino_link_init_mailbox(&link,&providers,&mailbox,10,100,100,100) == TUNUINO_LINK_RECEIVING);
    size_t consumed, length;
    text_query("TUNUINO:L");
    assert(tunuino_link_feed(&link,request,request_length,100,&consumed) == TUNUINO_LINK_RESPONDING);
    const uint8_t *body = drain(100,&length);
    assert(length == 1 && body[0] == 1);
    tunuino_live_sample sample = {100,1234000,100000,1000000,-12345,15};
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    assert(tunuino_link_feed(&link,request,1,109,&consumed) == TUNUINO_LINK_RECEIVING && consumed == 1);
    assert(tunuino_link_feed(&link,request + 1,request_length - 1,110,&consumed) == TUNUINO_LINK_RESPONDING);
    body = drain(110,&length); assert(length == 1 && body[0] == 1);
    /* Freshness is checked at completion; a response already built is immutable. */
    sample.time_ms = 110;
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    assert(tunuino_link_feed(&link,request,request_length,110,&consumed) == TUNUINO_LINK_RESPONDING);
    sample.valid_mask = 0;
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    body = drain(111,&length); assert(length == 28 && body[2] == 15);
    assert(tunuino_link_feed(&link,request,request_length,111,&consumed) == TUNUINO_LINK_RESPONDING);
    body = drain(111,&length); assert(length == 1 && body[0] == 1);
    sample.valid_mask = 15; sample.time_ms = 112;
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    assert(tunuino_link_feed(&link,request,request_length,112,&consumed) == TUNUINO_LINK_RESPONDING);
    body = drain(112,&length); assert(length == 28 && body[2] == 15);
    sample.time_ms = 120;
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    assert(tunuino_link_feed(&link,request,request_length,113,&consumed) == TUNUINO_LINK_RESPONDING);
    body = drain(113,&length); assert(length == 1 && body[0] == 1);
    assert(tunuino_link_feed(&link,request,request_length,120,&consumed) == TUNUINO_LINK_RESPONDING);
    body = drain(120,&length); assert(length == 28);
    assert(tunuino_link_feed(&link,request,request_length,130,&consumed) == TUNUINO_LINK_RESPONDING);
    body = drain(130,&length); assert(length == 1 && body[0] == 1);
    text_query("TUNUINO?");
    assert(tunuino_link_feed(&link,request,request_length,130,&consumed) == TUNUINO_LINK_RESPONDING);
    body = drain(130,&length); assert(length == 32);
    uint8_t map_query[] = {'T','U','N','U','I','N','O',':','M',0,1};
    query(map_query,sizeof(map_query));
    assert(tunuino_link_feed(&link,request,request_length,130,&consumed) == TUNUINO_LINK_RESPONDING);
    body = drain(130,&length); assert(length == 14);
    assert(tunuino_link_init_mailbox(NULL,&providers,&mailbox,10,100,100,0) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_init_mailbox(&link,NULL,&mailbox,10,100,100,0) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_init_mailbox(&link,&providers,NULL,10,100,100,0) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_init_mailbox(&link,&providers,&mailbox,0,100,100,0) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_init_mailbox(&link,&providers,&unconfigured,10,100,100,0) == TUNUINO_LINK_ARGUMENT);
    sticky(TUNUINO_LINK_ARGUMENT,0);
    identity.hardware_revision = 0;
    assert(tunuino_link_init_mailbox(&link,&providers,&mailbox,10,100,100,0) == TUNUINO_LINK_DISPATCH_ERROR);
    identity.hardware_revision = 1;
    init(100,100,0); text_query("TUNUINO:L");
    assert(tunuino_link_feed(&link,request,request_length,0,&consumed) == TUNUINO_LINK_RESPONDING);
    body = drain(0,&length); assert(length == 28); /* Ordinary re-init unbinds. */
    puts("C mailbox link: freshness at request completion, immutable TX, expiry/recovery and unaffected identity/maps passed");
}
int main(void) {
    map.rpm_count = map.load_count = 32;
    for (unsigned i = 0; i < 32; ++i) {
        map.rpm[i] = (int32_t)(i * 250); map.load_milli_kpa[i] = (int32_t)(i * 10000);
        for (unsigned j = 0; j < 32; ++j) { map.cells[i][j] = (int32_t)(i * 100 + j); }
    }
    map.kind = TUNUINO_MAP_VE;
    assert(tunuino_map_snapshot_prepare(&ve, &map, 7) == TUNUINO_MAP_OK);
    map.kind = TUNUINO_MAP_IGNITION; map.cells[0][0] = -12345;
    assert(tunuino_map_snapshot_prepare(&ignition, &map, 7) == TUNUINO_MAP_OK);
    size_t consumed, length;
    for (unsigned family = 1; family <= 2; ++family) {
        identity.family = (tunuino_family)family;
        init(100,100,0);
        text_query("TUNUINO?");
        for (size_t offset = 0; offset < request_length; ++offset) {
            assert(tunuino_link_feed(&link, request + offset, 1, 0, &consumed) == (offset + 1 == request_length ? TUNUINO_LINK_RESPONDING : TUNUINO_LINK_RECEIVING));
            assert(consumed == 1);
        }
        const uint8_t *body = drain(0,&length);
        assert(length == 32 && body[9] == family);
        text_query("TUNUINO:L");
        assert(tunuino_link_feed(&link, request, request_length, 0, &consumed) == TUNUINO_LINK_RESPONDING);
        body = drain(0,&length); assert(length == 28 && body[2] == 15);
        live.valid_mask = 0;
        assert(tunuino_link_feed(&link, request, request_length, 0, &consumed) == TUNUINO_LINK_RESPONDING);
        body = drain(0,&length); assert(length == 1 && body[0] == 1);
        live.valid_mask = 15;
    }
    for (unsigned kind = 1; kind <= 2; ++kind) {
        uint8_t q[19] = {'T','U','N','U','I','N','O',':','M',0,0,0,0,0,7,0,0,0,0};
        q[10] = (uint8_t)kind; query(q,11);
        assert(tunuino_link_feed(&link, request, request_length, 0, &consumed) == TUNUINO_LINK_RESPONDING);
        const uint8_t *body = drain(0,&length);
        assert(length == 14 && body[3] == kind && body[7] == 7 && body[8] == 0x11 && body[9] == 0x10);
        size_t offset = 0;
        while (offset < sizeof(file)) {
            size_t count = sizeof(file) - offset; if (count > 256) { count = 256; }
            q[9] = 1; q[15] = (uint8_t)(offset >> 8); q[16] = (uint8_t)offset;
            q[17] = (uint8_t)(count >> 8); q[18] = (uint8_t)count; query(q,19);
            assert(tunuino_link_feed(&link, request, request_length, 0, &consumed) == TUNUINO_LINK_RESPONDING && consumed == request_length);
            body = drain(0,&length); assert(length == 12 + count && body[2] == 1 && body[3] == kind);
            memcpy(file + offset,body + 12,count); offset += count;
        }
        const tunuino_map_snapshot *snapshot = kind == 1 ? &ve : &ignition;
        assert(memcmp(file,snapshot->bytes,sizeof(file)) == 0);
    }
    /* Coalesced input remains owned by caller, with no second dispatch yet. */
    text_query("TUNUINO?"); uint8_t combined[64]; size_t first = request_length;
    memcpy(combined,request,first); text_query("TUNUINO:L"); memcpy(combined + first,request,request_length);
    assert(tunuino_link_feed(&link,combined,first + request_length,0,&consumed) == TUNUINO_LINK_RESPONDING && consumed == first);
    const uint8_t *body = drain(0,&length); assert(length == 32);
    assert(tunuino_link_feed(&link,combined + first,request_length,0,&consumed) == TUNUINO_LINK_RESPONDING && consumed == request_length);
    body = drain(0,&length); assert(length == 28);
    /* Provider changes do not mutate a response already copied for sending. */
    text_query("TUNUINO?");
    assert(tunuino_link_feed(&link,request,request_length,0,&consumed) == TUNUINO_LINK_RESPONDING);
    identity.hardware_revision = 0;
    body = drain(0,&length); assert(length == 32 && body[12] == 1);
    assert(tunuino_link_feed(&link,request,request_length,0,&consumed) == TUNUINO_LINK_DISPATCH_ERROR);
    assert(link.dispatch_status == TUNUINO_DISPATCH_UNCONFIGURED); sticky(TUNUINO_LINK_DISPATCH_ERROR,0);
    identity.hardware_revision = 1;
    init(10,10,0);
    assert(tunuino_link_poll(&link,1000) == TUNUINO_LINK_RECEIVING);
    assert(tunuino_link_feed(&link,request,1,1000,&consumed) == TUNUINO_LINK_RECEIVING);
    assert(tunuino_link_poll(&link,1010) == TUNUINO_LINK_RX_ERROR && link.rx_status == TUNUINO_RX_TIMEOUT);
    sticky(TUNUINO_LINK_RX_ERROR,1011);
    init(10,10,0);
    assert(tunuino_link_feed(&link,request,request_length,0,&consumed) == TUNUINO_LINK_RESPONDING);
    assert(tunuino_link_tx_advance(&link,1,9) == TUNUINO_LINK_RESPONDING);
    assert(tunuino_link_poll(&link,10) == TUNUINO_LINK_TX_ERROR && link.tx_status == TUNUINO_TX_TIMEOUT);
    sticky(TUNUINO_LINK_TX_ERROR,11);
    init(10,10,0);
    assert(tunuino_link_feed(&link,request,request_length,0,&consumed) == TUNUINO_LINK_RESPONDING);
    assert(tunuino_link_tx_advance(&link,999,0) == TUNUINO_LINK_TX_ERROR && link.tx_status == TUNUINO_TX_COUNT);
    sticky(TUNUINO_LINK_TX_ERROR,0);
    init(10,10,100);
    assert(tunuino_link_poll(&link,99) == TUNUINO_LINK_RX_ERROR && link.rx_status == TUNUINO_RX_CLOCK);
    init(10,10,100);
    assert(tunuino_link_feed(&link,request,request_length,100,&consumed) == TUNUINO_LINK_RESPONDING);
    assert(tunuino_link_tx_advance(&link,1,99) == TUNUINO_LINK_TX_ERROR && link.tx_status == TUNUINO_TX_CLOCK);
    init(10,10,0); request[request_length - 1] ^= 1;
    assert(tunuino_link_feed(&link,request,request_length,0,&consumed) == TUNUINO_LINK_RX_ERROR && link.rx_status == TUNUINO_RX_CRC);
    sticky(TUNUINO_LINK_RX_ERROR,0); request[request_length - 1] ^= 1;
    init(10,10,0); text_query("unknown");
    assert(tunuino_link_feed(&link,request,request_length,0,&consumed) == TUNUINO_LINK_DISPATCH_ERROR && link.dispatch_status == TUNUINO_DISPATCH_UNKNOWN_REQUEST);
    sticky(TUNUINO_LINK_DISPATCH_ERROR,0);
    identity.capabilities = 0; init(10,10,0); text_query("TUNUINO:L");
    assert(tunuino_link_feed(&link,request,request_length,0,&consumed) == TUNUINO_LINK_DISPATCH_ERROR && link.dispatch_status == TUNUINO_DISPATCH_UNSUPPORTED);
    sticky(TUNUINO_LINK_DISPATCH_ERROR,0); identity.capabilities = 3;
    for (unsigned responding = 0; responding <= 1; ++responding) {
        init(10,10,0); text_query("TUNUINO?");
        assert(tunuino_link_feed(&link,request,responding ? request_length : 1,0,&consumed) == (responding ? TUNUINO_LINK_RESPONDING : TUNUINO_LINK_RECEIVING));
        tunuino_link_abort(&link); sticky(TUNUINO_LINK_CLOSED,0);
    }
    assert(tunuino_link_init(NULL,&context,10,10,0) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_init(&link,NULL,10,10,0) == TUNUINO_LINK_ARGUMENT);
    sticky(TUNUINO_LINK_ARGUMENT,0);
    assert(tunuino_link_init(&link,&context,0,10,0) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_init(&link,&context,10,60001,0) == TUNUINO_LINK_ARGUMENT);
    identity.capabilities = 4;
    assert(tunuino_link_init(&link,&context,10,10,0) == TUNUINO_LINK_DISPATCH_ERROR);
    identity.capabilities = 3; init(10,10,0);
    assert(tunuino_link_feed(&link,NULL,1,0,&consumed) == TUNUINO_LINK_ARGUMENT && consumed == 0);
    assert(tunuino_link_feed(&link,request,1,0,NULL) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_feed(NULL,request,1,0,&consumed) == TUNUINO_LINK_ARGUMENT && consumed == 0);
    assert(tunuino_link_poll(NULL,0) == TUNUINO_LINK_ARGUMENT);
    const uint8_t *view; size_t remaining;
    assert(tunuino_link_tx_view(NULL,0,&view,&remaining) == TUNUINO_LINK_ARGUMENT && view == NULL && remaining == 0);
    assert(tunuino_link_tx_view(&link,0,NULL,&remaining) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_tx_view(&link,0,&view,NULL) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_tx_advance(NULL,1,0) == TUNUINO_LINK_ARGUMENT);
    assert(tunuino_link_tx_advance(&link,1,0) == TUNUINO_LINK_ARGUMENT && link.state == TUNUINO_LINK_RECEIVING);
    tunuino_link_abort(NULL);
    mailbox_checks();
    puts("C link: serialized RX/dispatch/TX, coalesced requests, full VE/ignition reads, provider isolation and terminal errors passed");
    return 0;
}
