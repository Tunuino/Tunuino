/* Copyright (c) 2026 Tunuino. */
#include "tunuino/stream_tx.h"
#include "tunuino/stream_rx.h"
#include "tunuino/dispatch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tunuino_stream_tx tx;
static tunuino_stream_rx rx;
static uint8_t body[TUNUINO_MAX_BODY], packet[TUNUINO_MAX_BODY + TUNUINO_FRAME_OVERHEAD];
static uint8_t original[sizeof(packet)], wire[sizeof(packet)];
static void hidden(tunuino_tx_status status, uint64_t now) {
    const uint8_t *view = body; size_t count = 123;
    assert(tunuino_tx_view(&tx, now, &view, &count) == status && view == NULL && count == 0);
}
static void sticky(tunuino_tx_status status, uint64_t now, size_t length) {
    assert(tunuino_tx_poll(&tx, now) == status);
    assert(tunuino_tx_begin(&tx, packet, length, now) == status);
    assert(tunuino_tx_advance(&tx, 1, now) == status);
    hidden(status, now);
}
int main(void) {
    size_t length;
    for (unsigned i = 0; i < sizeof(body); ++i) { body[i] = (uint8_t)(i * 19u); }
    const size_t sizes[] = {1, 8, 28, 268, 4096};
    const size_t chunks[] = {1, 2, 3, 64, 255, 256, 4095, 4102};
    for (unsigned n = 0; n < sizeof(sizes) / sizeof(sizes[0]); ++n) {
        assert(tunuino_frame_encode(body, sizes[n], packet, sizeof(packet), &length) == TUNUINO_FRAME_OK);
        memcpy(original, packet, length);
        for (unsigned c = 0; c < sizeof(chunks) / sizeof(chunks[0]); ++c) {
            assert(tunuino_tx_init(&tx, 100, 0) == TUNUINO_TX_IDLE);
            hidden(TUNUINO_TX_IDLE, 1000000);
            assert(tunuino_tx_begin(&tx, packet, length, 1000000) == TUNUINO_TX_PENDING);
            /* The source can be reused immediately, independently of the driver. */
            memset(packet, 0, length);
            assert(tunuino_tx_begin(&tx, packet, length, 1000000) == TUNUINO_TX_BUSY);
            size_t sent = 0;
            while (sent < length) {
                const uint8_t *view; size_t remaining;
                assert(tunuino_tx_view(&tx, 1000001, &view, &remaining) == TUNUINO_TX_PENDING);
                assert(remaining == length - sent && memcmp(view, original + sent, remaining) == 0);
                assert(tunuino_tx_advance(&tx, 0, 1000001) == TUNUINO_TX_PENDING);
                size_t count = chunks[c] < remaining ? chunks[c] : remaining;
                memcpy(wire + sent, view, count);
                sent += count;
                assert(tunuino_tx_advance(&tx, count, 1000001) == (sent == length ? TUNUINO_TX_DONE : TUNUINO_TX_PENDING));
            }
            assert(memcmp(wire, original, length) == 0);
            hidden(TUNUINO_TX_DONE, 2000000);
            assert(tunuino_tx_advance(&tx, 1, 2000000) == TUNUINO_TX_ARGUMENT);
            memcpy(packet, original, length);
            assert(tunuino_tx_begin(&tx, packet, length, 2000000) == TUNUINO_TX_PENDING);
            assert(tunuino_tx_advance(&tx, length, 2000001) == TUNUINO_TX_DONE);
        }
    }
    /* A fresh frame is never appended to a partial failed response. */
    assert(tunuino_tx_init(&tx, 10, 0) == TUNUINO_TX_IDLE);
    assert(tunuino_tx_begin(&tx, packet, length, 0) == TUNUINO_TX_PENDING);
    assert(tunuino_tx_advance(&tx, 1, 9) == TUNUINO_TX_PENDING);
    assert(tunuino_tx_advance(&tx, length - 1, 10) == TUNUINO_TX_TIMEOUT);
    sticky(TUNUINO_TX_TIMEOUT, 11, length);
    assert(tunuino_tx_init(&tx, 10, 0) == TUNUINO_TX_IDLE);
    assert(tunuino_tx_begin(&tx, packet, length, 0) == TUNUINO_TX_PENDING);
    assert(tunuino_tx_advance(&tx, 0, 9) == TUNUINO_TX_PENDING);
    assert(tunuino_tx_poll(&tx, 10) == TUNUINO_TX_TIMEOUT);
    sticky(TUNUINO_TX_TIMEOUT, 11, length);
    assert(tunuino_tx_init(&tx, 10, 0) == TUNUINO_TX_IDLE);
    assert(tunuino_tx_begin(&tx, packet, length, 0) == TUNUINO_TX_PENDING);
    assert(tunuino_tx_advance(&tx, 1, 1) == TUNUINO_TX_PENDING);
    assert(tunuino_tx_advance(&tx, length, 1) == TUNUINO_TX_COUNT);
    sticky(TUNUINO_TX_COUNT, 2, length);
    assert(tunuino_tx_init(&tx, 10, 10) == TUNUINO_TX_IDLE);
    assert(tunuino_tx_poll(&tx, 9) == TUNUINO_TX_CLOCK);
    sticky(TUNUINO_TX_CLOCK, 11, length);
    assert(tunuino_tx_init(&tx, 10, UINT64_MAX - 10) == TUNUINO_TX_IDLE);
    assert(tunuino_tx_begin(&tx, packet, length, UINT64_MAX - 9) == TUNUINO_TX_PENDING);
    assert(tunuino_tx_advance(&tx, length, UINT64_MAX - 1) == TUNUINO_TX_DONE);
    assert(tunuino_tx_poll(&tx, UINT64_MAX) == TUNUINO_TX_DONE);
    assert(tunuino_tx_begin(&tx, packet, length, 0) == TUNUINO_TX_CLOCK);
    sticky(TUNUINO_TX_CLOCK, 1, length);
    for (unsigned pending = 0; pending <= 1; ++pending) {
        assert(tunuino_tx_init(&tx, 100, 0) == TUNUINO_TX_IDLE);
        if (pending) {
            assert(tunuino_tx_begin(&tx, packet, length, 0) == TUNUINO_TX_PENDING);
            assert(tunuino_tx_advance(&tx, 2, 0) == TUNUINO_TX_PENDING);
        }
        tunuino_tx_abort(&tx); sticky(TUNUINO_TX_CLOSED, 1, length);
    }
    assert(tunuino_tx_init(&tx, 100, 0) == TUNUINO_TX_IDLE);
    for (size_t truncated = 0; truncated < length; ++truncated) {
        assert(tunuino_tx_begin(&tx, packet, truncated, 0) == TUNUINO_TX_FRAME);
        hidden(TUNUINO_TX_IDLE, 0);
    }
    packet[length - 1] ^= 1;
    assert(tunuino_tx_begin(&tx, packet, length, 0) == TUNUINO_TX_FRAME);
    packet[length - 1] ^= 1;
    uint8_t invalid[7] = {0,0,0,0,0,0,0};
    assert(tunuino_tx_begin(&tx, invalid, sizeof(invalid), 0) == TUNUINO_TX_FRAME);
    invalid[0] = 0xFF; invalid[1] = 0xFF;
    assert(tunuino_tx_begin(&tx, invalid, sizeof(invalid), 0) == TUNUINO_TX_FRAME);
    assert(tunuino_tx_init(NULL, 100, 0) == TUNUINO_TX_ARGUMENT);
    assert(tunuino_tx_init(&tx, 0, 0) == TUNUINO_TX_ARGUMENT);
    sticky(TUNUINO_TX_ARGUMENT, 0, length);
    assert(tunuino_tx_init(&tx, 60001, 0) == TUNUINO_TX_ARGUMENT);
    sticky(TUNUINO_TX_ARGUMENT, 0, length);
    assert(tunuino_tx_init(&tx, 60000, 0) == TUNUINO_TX_IDLE);
    assert(tunuino_tx_poll(NULL, 0) == TUNUINO_TX_ARGUMENT);
    assert(tunuino_tx_begin(NULL, packet, length, 0) == TUNUINO_TX_ARGUMENT);
    assert(tunuino_tx_begin(&tx, NULL, length, 0) == TUNUINO_TX_ARGUMENT);
    const uint8_t *view; size_t remaining;
    assert(tunuino_tx_view(NULL, 0, &view, &remaining) == TUNUINO_TX_ARGUMENT && view == NULL && remaining == 0);
    assert(tunuino_tx_view(&tx, 0, NULL, &remaining) == TUNUINO_TX_ARGUMENT);
    assert(tunuino_tx_view(&tx, 0, &view, NULL) == TUNUINO_TX_ARGUMENT);
    assert(tunuino_tx_advance(NULL, 1, 0) == TUNUINO_TX_ARGUMENT);
    assert(tunuino_tx_advance(&tx, 0, 0) == TUNUINO_TX_ARGUMENT);
    tunuino_tx_abort(NULL);
    /* Full portable path: fragmented request -> dispatcher -> partial response. */
    tunuino_identity identity = {TUNUINO_PROTEUS, TUNUINO_STM32_H7, 2, 0, 1, 0, 0, {1,2,3,4,5,6,7,8,9,10,11,12}};
    tunuino_dispatch_context context = {&identity, NULL, NULL, NULL};
    assert(tunuino_frame_encode((const uint8_t *)"TUNUINO?", 8, packet, sizeof(packet), &length) == TUNUINO_FRAME_OK);
    assert(tunuino_rx_init(&rx, 100, 0) == TUNUINO_RX_MORE);
    size_t consumed;
    for (size_t offset = 0; offset < length; ++offset) {
        assert(tunuino_rx_feed(&rx, packet + offset, 1, offset, &consumed) == (offset + 1 == length ? TUNUINO_RX_READY : TUNUINO_RX_MORE));
    }
    assert(tunuino_rx_view(&rx, &view, &remaining) == TUNUINO_RX_READY);
    size_t written;
    assert(tunuino_dispatch_reply(&context, view, remaining, packet, sizeof(packet), &written) == TUNUINO_DISPATCH_OK);
    assert(tunuino_tx_begin(&tx, packet, written, 20) == TUNUINO_TX_PENDING);
    size_t sent = 0;
    while (sent < written) {
        assert(tunuino_tx_view(&tx, 21, &view, &remaining) == TUNUINO_TX_PENDING);
        size_t count = remaining > 3 ? 3 : remaining;
        memcpy(wire + sent, view, count); sent += count;
        assert(tunuino_tx_advance(&tx, count, 21) == (sent == written ? TUNUINO_TX_DONE : TUNUINO_TX_PENDING));
    }
    const uint8_t *answer; size_t answer_length;
    assert(tunuino_frame_decode(wire, sent, &answer, &answer_length) == TUNUINO_FRAME_OK);
    assert(answer_length == 32 && answer[9] == TUNUINO_PROTEUS && answer[10] == TUNUINO_STM32_H7);
    assert(tunuino_rx_release(&rx, 22) == TUNUINO_RX_MORE);
    puts("C stream transmission: partial completion, immutable copy, busy response, CRC, deadline, disconnect and full RX/dispatch/TX path passed");
    return 0;
}
