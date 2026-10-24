/* Copyright (c) 2026 Tunuino. */
#include "tunuino/live_data.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    /* Synthetic sensor values for host interoperability tests only. */
    tunuino_live_sample sample = {
        .time_ms = UINT64_C(0x123456789AB), .rpm_milli = 950125,
        .map_milli_kpa = 98765, .lambda_million = 1023456,
        .coolant_milli_c = -12345, .valid_mask = TUNUINO_LIVE_ALL_VALID
    };
    uint8_t request[32], response[64];
    size_t request_length, written;
    assert(tunuino_frame_encode((const uint8_t *)"TUNUINO:L", 9,
        request, sizeof(request), &request_length) == TUNUINO_FRAME_OK);
    assert(tunuino_live_reply(&sample, request, request_length, response, sizeof(response), &written) == TUNUINO_LIVE_OK);
    assert(written == 34);
    for (size_t i = 0; i < written; ++i) { printf("%02x", response[i]); }
    puts("");
    assert(tunuino_live_reply(&sample, request, request_length, response, 33, &written) == TUNUINO_LIVE_CAPACITY);
    assert(written == 0);
    for (unsigned valid = 0; valid < 15; ++valid) {
        sample.valid_mask = (uint8_t)valid;
        assert(tunuino_live_reply(&sample, request, request_length, response, sizeof(response), &written) == TUNUINO_LIVE_OK);
        const uint8_t *body;
        size_t length;
        assert(tunuino_frame_decode(response, written, &body, &length) == TUNUINO_FRAME_OK);
        assert(length == 1 && body[0] == 1);
    }
    sample.valid_mask = 0xFF;
    assert(tunuino_live_reply(&sample, request, request_length, response, sizeof(response), &written) == TUNUINO_LIVE_BAD_SAMPLE);
    sample.valid_mask = TUNUINO_LIVE_ALL_VALID;
    sample.time_ms = UINT64_MAX;
    assert(tunuino_live_reply(&sample, request, request_length, response, sizeof(response), &written) == TUNUINO_LIVE_BAD_SAMPLE);
    sample.time_ms = 0;
    for (size_t i = 0; i < request_length; ++i) {
        request[i] ^= 1;
        assert(tunuino_live_reply(&sample, request, request_length, response, sizeof(response), &written) == TUNUINO_LIVE_BAD_FRAME);
        assert(written == 0);
        request[i] ^= 1;
    }
    assert(tunuino_frame_encode((const uint8_t *)"X", 1, request, sizeof(request), &request_length) == TUNUINO_FRAME_OK);
    assert(tunuino_live_reply(&sample, request, request_length, response, sizeof(response), &written) == TUNUINO_LIVE_BAD_REQUEST);
    puts("C live-data layout and unavailable-sensor checks passed");
    return 0;
}
