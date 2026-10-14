// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_partial.h"
size_t tunuino_console_encode_partial(const tunuino_console_partial_t *data,
                                      uint8_t *out, size_t capacity) {
    if (!data || (data->valid_mask & (uint16_t)~0x7fu)) return 0u;
    const uint16_t values[7] = {
        data->fields.rpm, data->fields.map_tenths_kpa,
        data->fields.tps_tenths_percent, data->fields.clt_tenths_plus_40,
        data->fields.iat_tenths_plus_40, data->fields.afr_hundredths,
        data->fields.battery_hundredths_volts
    };
    uint8_t payload[16] = {0};
    payload[0] = (uint8_t)data->valid_mask;
    payload[1] = (uint8_t)(data->valid_mask >> 8);
    for (unsigned i = 0; i < 7; ++i) {
        if ((data->valid_mask & (1u << i)) == 0u) continue;
        payload[2u + 2u * i] = (uint8_t)values[i];
        payload[3u + 2u * i] = (uint8_t)(values[i] >> 8);
    }
    return tunuino_console_encode(TUNUINO_CONSOLE_TYPE_PARTIAL,
                                   payload, sizeof(payload), out, capacity);
}
