/* Copyright (c) 2026 Tunuino. */
#include "tunuino/frame.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const uint8_t check[] = "123456789";
    assert(tunuino_crc32(check, 9) == UINT32_C(0xCBF43926));
    uint8_t body[TUNUINO_MAX_BODY];
    uint8_t packet[TUNUINO_MAX_BODY + TUNUINO_FRAME_OVERHEAD];
    for (size_t i = 0; i < sizeof(body); ++i) {
        body[i] = (uint8_t)i;
    }
    const uint8_t *decoded = NULL;
    size_t written = 0, decoded_length = 0;
    const size_t sizes[] = {1, 255, 256, TUNUINO_MAX_BODY};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        size_t n = sizes[i];
        assert(tunuino_frame_encode(body, n, packet, sizeof(packet), &written) == TUNUINO_FRAME_OK);
        assert(tunuino_frame_decode(packet, written, &decoded, &decoded_length) == TUNUINO_FRAME_OK);
        assert(decoded_length == n && memcmp(body, decoded, n) == 0);
        assert(tunuino_frame_decode(packet, written - 1, &decoded, &decoded_length) == TUNUINO_FRAME_LENGTH);
        packet[2] ^= 1;
        assert(tunuino_frame_decode(packet, written, &decoded, &decoded_length) == TUNUINO_FRAME_CRC);
        assert(decoded == NULL && decoded_length == 0);
    }
    assert(tunuino_frame_encode(body, 0, packet, sizeof(packet), &written) == TUNUINO_FRAME_LENGTH);
    assert(tunuino_frame_encode(body, TUNUINO_MAX_BODY + 1u, packet, sizeof(packet), &written) == TUNUINO_FRAME_LENGTH);
    assert(tunuino_frame_encode(body, 1, packet, 6, &written) == TUNUINO_FRAME_CAPACITY);
    assert(written == 0);
    assert(tunuino_frame_encode(NULL, 1, packet, sizeof(packet), &written) == TUNUINO_FRAME_ARGUMENT);
    memcpy(packet, check, 9);
    assert(tunuino_frame_encode(packet, 9, packet, sizeof(packet), &written) == TUNUINO_FRAME_OK);
    assert(tunuino_frame_decode(packet, written, &decoded, &decoded_length) == TUNUINO_FRAME_OK);
    assert(memcmp(decoded, check, 9) == 0);
    /* Emit a deterministic vector for the independent Java implementation. */
    for (size_t i = 0; i < written; ++i) {
        printf("%02x", packet[i]);
    }
    puts("");
    return 0;
}
