// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_partial.h"
#include <assert.h>
int main(void) {
    tunuino_console_partial_t partial = {0};
    partial.valid_mask = TUNUINO_CONSOLE_FIELD_RPM;
    partial.fields.rpm = 1850u;
    partial.fields.map_tenths_kpa = 999u;
    uint8_t frame[32];
    const size_t n = tunuino_console_encode_partial(&partial, frame, sizeof(frame));
    assert(n == 24u && frame[3] == 3u && frame[4] == 16u);
    assert(frame[6] == 1u && frame[7] == 0u);
    assert(frame[8] == 0x3au && frame[9] == 0x07u);
    assert(frame[10] == 0u && frame[11] == 0u);
    partial.valid_mask = 0x80u;
    assert(tunuino_console_encode_partial(&partial, frame, sizeof(frame)) == 0u);
    return 0;
}
