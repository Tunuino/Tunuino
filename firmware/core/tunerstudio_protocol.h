// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TUNERSTUDIO_PROTOCOL_H
#define TUNUINO_TUNERSTUDIO_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
// Experimental read-only subset. No writes or ECU control.
#define TUNUINO_TS_SIGNATURE "Tunuino-TS-0.1"
#define TUNUINO_TS_OUTPUT_BYTES 8u
typedef struct {
    uint16_t rpm;
    uint16_t map_tenths_kpa;
    uint16_t battery_hundredths_v;
    uint8_t rpm_valid;
    uint8_t map_valid;
    uint8_t battery_valid;
} tunuino_ts_values_t;
// Returns number of response bytes; zero indicates unsupported command.
size_t tunuino_ts_reply(uint8_t command, const tunuino_ts_values_t *values,
                        uint8_t *out, size_t capacity);
#endif
