/* Copyright (c) 2026 Tunuino. */
#include "tunuino/stream_rx.h"
#include "tunuino/dispatch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tunuino_stream_rx rx;
static uint8_t body[TUNUINO_MAX_BODY], packet[TUNUINO_MAX_BODY + TUNUINO_FRAME_OVERHEAD];
static uint8_t combined[2 * (TUNUINO_MAX_BODY + TUNUINO_FRAME_OVERHEAD)];
static void check_view(const uint8_t *expected, size_t count) {
    const uint8_t *view; size_t length;
    assert(tunuino_rx_view(&rx, &view, &length) == TUNUINO_RX_READY);
    assert(length == count && memcmp(view, expected, count) == 0);
}
static void hidden(tunuino_rx_status status) {
    const uint8_t *view = body; size_t length = 123;
    assert(tunuino_rx_view(&rx, &view, &length) == status && view == NULL && length == 0);
}
static void sticky(tunuino_rx_status status, uint64_t now) {
    size_t consumed = 123;
    assert(tunuino_rx_poll(&rx, now) == status);
    assert(tunuino_rx_feed(&rx, packet, sizeof(packet), now, &consumed) == status && consumed == 0);
    assert(tunuino_rx_release(&rx, now) == status);
    hidden(status);
}
int main(void) {
    size_t length, consumed;
    for (unsigned i = 0; i < sizeof(body); ++i) { body[i] = (uint8_t)(i * 31u); }
    const size_t sizes[] = {1, 8, 9, 11, 19, 255, 256, 4096};
    for (unsigned n = 0; n < sizeof(sizes) / sizeof(sizes[0]); ++n) {
        assert(tunuino_frame_encode(body, sizes[n], packet, sizeof(packet), &length) == TUNUINO_FRAME_OK);
        /* Every possible split, including within the length prefix and CRC. */
        for (size_t split = 0; split <= length; ++split) {
            assert(tunuino_rx_init(&rx, 100, 0) == TUNUINO_RX_MORE);
            assert(tunuino_rx_feed(&rx, packet, split, 0, &consumed) == (split == length ? TUNUINO_RX_READY : TUNUINO_RX_MORE));
            assert(consumed == split);
            assert(tunuino_rx_feed(&rx, packet + split, length - split, 1, &consumed) == TUNUINO_RX_READY);
            assert(consumed == length - split);
            check_view(packet, length);
        }
        assert(tunuino_rx_init(&rx, 100, 0) == TUNUINO_RX_MORE);
        for (size_t offset = 0; offset < length; ++offset) {
            tunuino_rx_status state = tunuino_rx_feed(&rx, packet + offset, 1, 1, &consumed);
            assert(state == (offset + 1 == length ? TUNUINO_RX_READY : TUNUINO_RX_MORE) && consumed == 1);
        }
        check_view(packet, length);
        memcpy(combined, packet, length); memcpy(combined + length, packet, length);
        assert(tunuino_rx_init(&rx, 100, 0) == TUNUINO_RX_MORE);
        assert(tunuino_rx_feed(&rx, combined, length * 2, 1, &consumed) == TUNUINO_RX_READY && consumed == length);
        assert(tunuino_rx_feed(&rx, combined + length, length, 2, &consumed) == TUNUINO_RX_READY && consumed == 0);
        check_view(packet, length);
        assert(tunuino_rx_release(&rx, 2) == TUNUINO_RX_MORE);
        assert(tunuino_rx_feed(&rx, combined + length, length, 3, &consumed) == TUNUINO_RX_READY && consumed == length);
        check_view(packet, length);
        packet[length - 1] ^= 1;
        assert(tunuino_rx_init(&rx, 100, 0) == TUNUINO_RX_MORE);
        assert(tunuino_rx_feed(&rx, packet, length, 0, &consumed) == TUNUINO_RX_CRC && consumed == length);
        sticky(TUNUINO_RX_CRC, 1);
        packet[length - 1] ^= 1;
    }
    const unsigned invalid_sizes[] = {0, 4097, 65535};
    for (unsigned i = 0; i < sizeof(invalid_sizes) / sizeof(invalid_sizes[0]); ++i) {
        unsigned size = invalid_sizes[i];
        packet[0] = (uint8_t)(size >> 8); packet[1] = (uint8_t)size;
        assert(tunuino_rx_init(&rx, 100, 0) == TUNUINO_RX_MORE);
        assert(tunuino_rx_feed(&rx, packet, sizeof(packet), 1, &consumed) == TUNUINO_RX_LENGTH && consumed == 2);
        sticky(TUNUINO_RX_LENGTH, 2);
    }
    assert(tunuino_frame_encode(body, 8, packet, sizeof(packet), &length) == TUNUINO_FRAME_OK);
    /* No timeout while idle; the entire frame, not each fragment, gets 10 ms. */
    assert(tunuino_rx_init(&rx, 10, 0) == TUNUINO_RX_MORE);
    assert(tunuino_rx_poll(&rx, 1000000) == TUNUINO_RX_MORE);
    assert(tunuino_rx_feed(&rx, packet, 1, 1000000, &consumed) == TUNUINO_RX_MORE);
    assert(tunuino_rx_feed(&rx, packet + 1, 1, 1000009, &consumed) == TUNUINO_RX_MORE);
    assert(tunuino_rx_feed(&rx, packet + 2, length - 2, 1000010, &consumed) == TUNUINO_RX_TIMEOUT && consumed == 0);
    sticky(TUNUINO_RX_TIMEOUT, 1000020);
    /* Timer polling catches incomplete headers and bodies with no more input. */
    for (size_t partial = 1; partial < length; ++partial) {
        assert(tunuino_rx_init(&rx, 10, 100) == TUNUINO_RX_MORE);
        assert(tunuino_rx_feed(&rx, packet, partial, 100, &consumed) == TUNUINO_RX_MORE);
        hidden(TUNUINO_RX_MORE);
        assert(tunuino_rx_poll(&rx, 109) == TUNUINO_RX_MORE);
        assert(tunuino_rx_poll(&rx, 110) == TUNUINO_RX_TIMEOUT);
        sticky(TUNUINO_RX_TIMEOUT, 111);
    }
    assert(tunuino_rx_init(&rx, 10, UINT64_MAX - 10) == TUNUINO_RX_MORE);
    assert(tunuino_rx_feed(&rx, packet, 1, UINT64_MAX - 9, &consumed) == TUNUINO_RX_MORE);
    assert(tunuino_rx_feed(&rx, packet + 1, length - 1, UINT64_MAX - 1, &consumed) == TUNUINO_RX_READY);
    check_view(packet, length);
    assert(tunuino_rx_poll(&rx, UINT64_MAX) == TUNUINO_RX_READY);
    assert(tunuino_rx_release(&rx, 0) == TUNUINO_RX_CLOCK);
    sticky(TUNUINO_RX_CLOCK, 1);
    assert(tunuino_rx_init(&rx, 10, 100) == TUNUINO_RX_MORE);
    assert(tunuino_rx_poll(&rx, 99) == TUNUINO_RX_CLOCK);
    sticky(TUNUINO_RX_CLOCK, 101);
    for (unsigned ready = 0; ready <= 1; ++ready) {
        assert(tunuino_rx_init(&rx, 100, 0) == TUNUINO_RX_MORE);
        assert(tunuino_rx_feed(&rx, packet, ready ? length : 1, 0, &consumed) == (ready ? TUNUINO_RX_READY : TUNUINO_RX_MORE));
        tunuino_rx_abort(&rx); sticky(TUNUINO_RX_CLOSED, 1);
    }
    assert(tunuino_rx_init(NULL, 100, 0) == TUNUINO_RX_ARGUMENT);
    for (unsigned timeout = 0; timeout <= 60001; timeout += 60001) {
        assert(tunuino_rx_init(&rx, timeout, 0) == TUNUINO_RX_ARGUMENT);
        sticky(TUNUINO_RX_ARGUMENT, 0);
    }
    assert(tunuino_rx_init(&rx, 60000, 0) == TUNUINO_RX_MORE);
    assert(tunuino_rx_release(&rx, 0) == TUNUINO_RX_ARGUMENT);
    assert(tunuino_rx_feed(&rx, NULL, 1, 0, &consumed) == TUNUINO_RX_ARGUMENT && consumed == 0);
    assert(tunuino_rx_feed(&rx, packet, 1, 0, NULL) == TUNUINO_RX_ARGUMENT);
    assert(tunuino_rx_poll(NULL, 0) == TUNUINO_RX_ARGUMENT);
    assert(tunuino_rx_release(NULL, 0) == TUNUINO_RX_ARGUMENT);
    const uint8_t *view; size_t view_length;
    assert(tunuino_rx_view(NULL, &view, &view_length) == TUNUINO_RX_ARGUMENT && view == NULL && view_length == 0);
    assert(tunuino_rx_view(&rx, NULL, &view_length) == TUNUINO_RX_ARGUMENT);
    assert(tunuino_rx_view(&rx, &view, NULL) == TUNUINO_RX_ARGUMENT);
    tunuino_rx_abort(NULL);
    /* Fragmented request -> collector -> real dispatcher -> framed identity. */
    tunuino_identity identity = {TUNUINO_UAEFI, TUNUINO_STM32_F7, 2, 0, 1, 0, 0, {1,2,3,4,5,6,7,8,9,10,11,12}};
    tunuino_dispatch_context context = {&identity, NULL, NULL, NULL};
    assert(tunuino_frame_encode((const uint8_t *)"TUNUINO?", 8, packet, sizeof(packet), &length) == TUNUINO_FRAME_OK);
    assert(tunuino_rx_feed(&rx, packet, 3, 1, &consumed) == TUNUINO_RX_MORE);
    assert(tunuino_rx_feed(&rx, packet + 3, length - 3, 2, &consumed) == TUNUINO_RX_READY);
    assert(tunuino_rx_view(&rx, &view, &view_length) == TUNUINO_RX_READY);
    uint8_t response[64]; size_t written;
    assert(tunuino_dispatch_reply(&context, view, view_length, response, sizeof(response), &written) == TUNUINO_DISPATCH_OK);
    const uint8_t *answer; size_t answer_length;
    assert(tunuino_frame_decode(response, written, &answer, &answer_length) == TUNUINO_FRAME_OK);
    assert(answer_length == 32 && answer[9] == TUNUINO_UAEFI && answer[10] == TUNUINO_STM32_F7);
    assert(tunuino_rx_release(&rx, 3) == TUNUINO_RX_MORE);
    puts("C stream reception: all split points, coalesced frames, bounds, CRC, total deadline, disconnect and dispatcher integration passed");
    return 0;
}
