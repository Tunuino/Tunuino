// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_CONSOLE_SNAPSHOT_H
#define TUNUINO_CONSOLE_SNAPSHOT_H
#include <stdint.h>
#include "engine_state.h"
#include "console_protocol.h"
#define TUNUINO_SENSOR_MAP_VALID     (1u << 0)
#define TUNUINO_SENSOR_TPS_VALID     (1u << 1)
#define TUNUINO_SENSOR_CLT_VALID     (1u << 2)
#define TUNUINO_SENSOR_IAT_VALID     (1u << 3)
#define TUNUINO_SENSOR_AFR_VALID     (1u << 4)
#define TUNUINO_SENSOR_BATTERY_VALID (1u << 5)
#define TUNUINO_SENSOR_ALL_VALID     (0x3fu)
typedef struct {
    uint16_t map_tenths_kpa;
    uint16_t tps_tenths_percent;
    uint16_t clt_tenths_plus_40;
    uint16_t iat_tenths_plus_40;
    uint16_t afr_hundredths;
    uint16_t battery_hundredths_volts;
    uint8_t valid_mask;
} tunuino_console_sensors_t;
// Returns 1 only when RPM and every sensor field is valid and in range.
uint8_t tunuino_console_make_snapshot(const tunuino_engine_state_t *engine,
    const tunuino_console_sensors_t *sensors,
    tunuino_console_telemetry_t *out);
#endif
