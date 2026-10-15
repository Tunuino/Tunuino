// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_PROTOCOL_H
#define TUNUINO_CONSOLE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define TUNUINO_CONSOLE_PROTOCOL_VERSION 1u
#define TUNUINO_CONSOLE_MAX_PAYLOAD 64u
#define TUNUINO_CONSOLE_FRAME_OVERHEAD 8u
#define TUNUINO_CONSOLE_TYPE_HELLO 1u
#define TUNUINO_CONSOLE_TYPE_TELEMETRY 2u
typedef struct {
    uint16_t rpm;
    uint16_t map_tenths_kpa;
    uint16_t tps_tenths_percent;
    uint16_t clt_tenths_plus_40;
    uint16_t iat_tenths_plus_40;
    uint16_t afr_hundredths;
    uint16_t battery_hundredths_volts;
} tunuino_console_telemetry_t;
uint16_t tunuino_console_crc16(const uint8_t *data, size_t size);
size_t tunuino_console_encode(uint8_t type, const uint8_t *payload, size_t payload_size,
                              uint8_t *output, size_t capacity);
size_t tunuino_console_encode_telemetry(const tunuino_console_telemetry_t *data,
                                        uint8_t *output, size_t capacity);
#endif
