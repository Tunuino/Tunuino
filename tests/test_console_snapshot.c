// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_snapshot.h"
#include <assert.h>
int main(void) {
    tunuino_engine_state_t engine = {0};
    engine.engine_running = 1u;
    engine.speed.valid = 1u;
    engine.speed.rpm = 1500u;
    tunuino_console_sensors_t sensors = {
        1013u, 325u, 1250u, 650u, 1470u, 1380u, TUNUINO_SENSOR_ALL_VALID
    };
    tunuino_console_telemetry_t out = {0};
    assert(tunuino_console_make_snapshot(&engine, &sensors, &out));
    assert(out.rpm == 1500u && out.map_tenths_kpa == 1013u);
    sensors.valid_mask &= (uint8_t)~TUNUINO_SENSOR_AFR_VALID;
    assert(!tunuino_console_make_snapshot(&engine, &sensors, &out));
    sensors.valid_mask = TUNUINO_SENSOR_ALL_VALID;
    sensors.tps_tenths_percent = 1001u;
    assert(!tunuino_console_make_snapshot(&engine, &sensors, &out));
    sensors.tps_tenths_percent = 325u;
    engine.speed.valid = 0u;
    assert(!tunuino_console_make_snapshot(&engine, &sensors, &out));
    engine.speed.valid = 1u;
    engine.engine_running = 0u;
    assert(!tunuino_console_make_snapshot(&engine, &sensors, &out));
    engine.engine_running = 1u;
    engine.speed.rpm = 65536u;
    assert(!tunuino_console_make_snapshot(&engine, &sensors, &out));
    return 0;
}
