// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_protocol.h"
uint16_t tunuino_console_crc16(const uint8_t *data, size_t size) {
    uint16_t crc = 0xFFFFu;
    if (!data && size) return 0u;
    for (size_t i = 0; i < size; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; ++bit)
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)(((uint32_t)crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)((uint32_t)crc << 1);
            }
    }
    return crc;
}
size_t tunuino_console_encode(uint8_t type, const uint8_t *payload, size_t size,
                              uint8_t *out, size_t capacity) {
    if (!out || (size && !payload) || size > TUNUINO_CONSOLE_MAX_PAYLOAD ||
        capacity < size + TUNUINO_CONSOLE_FRAME_OVERHEAD) return 0u;
    out[0] = 0x54u; out[1] = 0x4Eu;
    out[2] = TUNUINO_CONSOLE_PROTOCOL_VERSION;
    out[3] = type;
    out[4] = (uint8_t)size; out[5] = (uint8_t)(size >> 8);
    for (size_t i = 0; i < size; ++i) out[6u + i] = payload[i];
    const uint16_t crc = tunuino_console_crc16(out + 2u, size + 4u);
    out[6u + size] = (uint8_t)crc;
    out[7u + size] = (uint8_t)(crc >> 8);
    return size + TUNUINO_CONSOLE_FRAME_OVERHEAD;
}
size_t tunuino_console_encode_telemetry(const tunuino_console_telemetry_t *data,
                                        uint8_t *out, size_t capacity) {
    if (!data) return 0u;
    const uint16_t fields[7] = {data->rpm, data->map_tenths_kpa,
        data->tps_tenths_percent, data->clt_tenths_plus_40,
        data->iat_tenths_plus_40, data->afr_hundredths, data->battery_hundredths_volts};
    uint8_t payload[14];
    for (unsigned i = 0; i < 7; ++i) {
        payload[i * 2u] = (uint8_t)fields[i];
        payload[i * 2u + 1u] = (uint8_t)(fields[i] >> 8);
    }
    return tunuino_console_encode(TUNUINO_CONSOLE_TYPE_TELEMETRY, payload, sizeof(payload), out, capacity);
}
