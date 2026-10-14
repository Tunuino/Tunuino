// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "console_snapshot.h"
uint8_t tunuino_console_make_snapshot(const tunuino_engine_state_t *engine,
    const tunuino_console_sensors_t *sensors,
    tunuino_console_telemetry_t *out) {
    if (!engine || !sensors || !out) return 0u;
    if (!engine->engine_running || !engine->speed.valid ||
        engine->speed.rpm == 0u || engine->speed.rpm > 65535u ||
        (sensors->valid_mask & TUNUINO_SENSOR_ALL_VALID) != TUNUINO_SENSOR_ALL_VALID)
        return 0u;
    if (sensors->map_tenths_kpa > 4000u ||
        sensors->tps_tenths_percent > 1000u ||
        sensors->clt_tenths_plus_40 > 2550u ||
        sensors->iat_tenths_plus_40 > 2550u ||
        sensors->afr_hundredths < 500u || sensors->afr_hundredths > 3000u ||
        sensors->battery_hundredths_volts > 3000u)
        return 0u;
    const tunuino_console_telemetry_t snapshot = {
        (uint16_t)engine->speed.rpm,
        sensors->map_tenths_kpa,
        sensors->tps_tenths_percent,
        sensors->clt_tenths_plus_40,
        sensors->iat_tenths_plus_40,
        sensors->afr_hundredths,
        sensors->battery_hundredths_volts
    };
    *out = snapshot;
    return 1u;
}
