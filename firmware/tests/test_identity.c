/* Copyright (c) 2026 Tunuino. */
#include "tunuino/identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* Synthetic identity used only by host tests, never a production board map. */
    tunuino_identity identity = {
        .family = TUNUINO_PROTEUS, .mcu_series = TUNUINO_STM32_F4,
        .hardware_revision = 0x0102, .firmware_major = 0, .firmware_minor = 1,
        .firmware_patch = 3, .capabilities = 0,
        .uid = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}
    };
    uint8_t request[32], response[64];
    size_t request_size = 0, response_size = 0;
    const uint8_t query[] = "TUNUINO?";
    assert(tunuino_frame_encode(query, 8, request, sizeof(request), &request_size) == TUNUINO_FRAME_OK);
    assert(tunuino_identity_reply(&identity, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_OK);
    assert(response_size == TUNUINO_IDENTITY_BODY_SIZE + TUNUINO_FRAME_OVERHEAD);
    for (size_t i = 0; i < response_size; ++i) {
        printf("%02x", response[i]);
    }
    puts("");
    const uint8_t *body;
    size_t body_size;
    assert(tunuino_frame_decode(response, response_size, &body, &body_size) == TUNUINO_FRAME_OK);
    assert(body_size == 32 && body[0] == 0 && memcmp(body + 1, "TUNUINO", 7) == 0);
    assert(body[11] == 1 && body[12] == 2 && memcmp(body + 20, identity.uid, 12) == 0);
    for (size_t i = 0; i < request_size; ++i) {
        request[i] ^= 1;
        assert(tunuino_identity_reply(&identity, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_BAD_FRAME);
        assert(response_size == 0);
        request[i] ^= 1;
    }
    assert(tunuino_identity_reply(&identity, request, request_size, response, 37, &response_size) == TUNUINO_IDENTITY_CAPACITY);
    assert(response_size == 0);
    identity.hardware_revision = 0;
    assert(tunuino_identity_reply(&identity, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_UNCONFIGURED);
    identity.hardware_revision = 1;
    identity.capabilities = UINT32_C(0x80000000);
    assert(tunuino_identity_reply(&identity, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_UNCONFIGURED);
    identity.capabilities = 0;
    memset(identity.uid, 0, sizeof(identity.uid));
    assert(tunuino_identity_reply(&identity, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_UNCONFIGURED);
    memset(identity.uid, 0xFF, sizeof(identity.uid));
    assert(tunuino_identity_reply(&identity, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_UNCONFIGURED);
    identity.uid[0] = 1;
    identity.family = (tunuino_family)0;
    assert(tunuino_identity_reply(&identity, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_UNCONFIGURED);
    identity.family = TUNUINO_UAEFI;
    identity.mcu_series = TUNUINO_STM32_H7;
    assert(tunuino_identity_reply(&identity, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_OK);
    assert(tunuino_frame_encode((const uint8_t *)"X", 1, request, sizeof(request), &request_size) == TUNUINO_FRAME_OK);
    assert(tunuino_identity_reply(&identity, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_BAD_REQUEST);
    assert(tunuino_identity_reply(NULL, request, request_size, response, sizeof(response), &response_size) == TUNUINO_IDENTITY_ARGUMENT);
    puts("C identity validation checks passed");
    return 0;
}
