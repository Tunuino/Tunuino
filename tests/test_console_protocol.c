// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_protocol.h"
#include <assert.h>
#include <string.h>
int main(void) {
    const uint8_t vector[] = "123456789";
    assert(tunuino_console_crc16(vector, 9u) == 0x29B1u);
    uint8_t output[32];
    const uint8_t payload[] = {0u, 1u, 255u, 32u};
    const size_t len = tunuino_console_encode(2u, payload, sizeof(payload), output, sizeof(output));
    assert(len == 12u);
    assert(output[0] == 0x54u && output[1] == 0x4Eu && output[3] == 2u);
    assert(memcmp(output + 6, payload, sizeof(payload)) == 0);
    assert(tunuino_console_encode(2u, payload, sizeof(payload), output, 4u) == 0u);
    const tunuino_console_telemetry_t t = {2500u, 1013u, 275u, 1250u, 700u, 1470u, 1380u};
    assert(tunuino_console_encode_telemetry(&t, output, sizeof(output)) == 22u);
    assert(output[6] == (2500u & 255u) && output[7] == (2500u >> 8));
    return 0;
}
