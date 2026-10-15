// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "tunerstudio_protocol.h"
#include <string.h>
static void put16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xffu);
    out[1] = (uint8_t)(value >> 8u);
}
size_t tunuino_ts_reply(uint8_t command, const tunuino_ts_values_t *values,
                        uint8_t *out, size_t capacity) {
    if (!out) return 0u;
    if (command == (uint8_t)'S') {
        static const char signature[] = TUNUINO_TS_SIGNATURE;
        const size_t size = sizeof(signature) - 1u;
        if (capacity < size) return 0u;
        memcpy(out, signature, size);
        return size;
    }
    if (command == (uint8_t)'A' && values && capacity >= TUNUINO_TS_OUTPUT_BYTES) {
        // Fixed little-endian output channel layout for prototype .ini.
        // Invalid channels are represented by 0xffff, not invented measurements.
        put16(out, values->rpm_valid ? values->rpm : UINT16_MAX);
        put16(out + 2u, values->map_valid ? values->map_tenths_kpa : UINT16_MAX);
        put16(out + 4u, values->battery_valid ? values->battery_hundredths_v : UINT16_MAX);
        put16(out + 6u, 0u); // reserved
        return TUNUINO_TS_OUTPUT_BYTES;
    }
    return 0u;
}
