/* Copyright (c) 2026 Tunuino. */
#include "tunuino/analog.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
    /* Synthetic fixtures, not curves for physical sensors. */
    const tunuino_analog_point points[] = {{100, -10000}, {200, 0}, {400, 60000}};
    tunuino_analog_curve curve = {points, 3, -10000, 60000};
    int32_t value = 99;
    for (uint16_t raw = 100; raw <= 400; ++raw) {
        assert(tunuino_analog_convert(&curve, raw, true, &value) == TUNUINO_ANALOG_OK);
        assert(value == (raw <= 200 ? ((int32_t)raw - 200) * 100 : ((int32_t)raw - 200) * 300));
    }
    assert(tunuino_analog_convert(&curve, 99, true, &value) == TUNUINO_ANALOG_OUT_OF_RANGE && value == 0);
    assert(tunuino_analog_convert(&curve, 401, true, &value) == TUNUINO_ANALOG_OUT_OF_RANGE && value == 0);
    assert(tunuino_analog_convert(&curve, 200, false, &value) == TUNUINO_ANALOG_INVALID_SAMPLE && value == 0);
    assert(tunuino_analog_convert(NULL, 200, true, &value) == TUNUINO_ANALOG_CONFIGURATION && value == 0);
    assert(tunuino_analog_convert(&curve, 200, true, NULL) == TUNUINO_ANALOG_CONFIGURATION);
    const tunuino_analog_point rounding[] = {{0, -1}, {4, 1}};
    curve = (tunuino_analog_curve){rounding, 2, -1, 1};
    const int32_t expected[] = {-1, -1, 0, 1, 1};
    for (uint16_t raw = 0; raw < 5; ++raw) {
        assert(tunuino_analog_convert(&curve, raw, true, &value) == TUNUINO_ANALOG_OK);
        assert(value == expected[raw]);
    }
    const tunuino_analog_point extremes[] = {{0, INT32_MAX}, {UINT16_MAX, INT32_MIN}};
    curve = (tunuino_analog_curve){extremes, 2, INT32_MIN, INT32_MAX};
    assert(tunuino_analog_convert(&curve, 0, true, &value) == TUNUINO_ANALOG_OK && value == INT32_MAX);
    assert(tunuino_analog_convert(&curve, UINT16_MAX, true, &value) == TUNUINO_ANALOG_OK && value == INT32_MIN);
    assert(tunuino_analog_convert(&curve, 32767, true, &value) == TUNUINO_ANALOG_OK && value == 32768);
    assert(tunuino_analog_convert(&curve, 32768, true, &value) == TUNUINO_ANALOG_OK && value == -32769);
    tunuino_analog_point bad[] = {{1, 0}, {1, 1}};
    curve = (tunuino_analog_curve){bad, 2, 0, 1};
    assert(tunuino_analog_convert(&curve, 1, true, &value) == TUNUINO_ANALOG_CONFIGURATION && value == 0);
    bad[1].raw = 0;
    assert(tunuino_analog_convert(&curve, 1, true, &value) == TUNUINO_ANALOG_CONFIGURATION);
    bad[1].raw = 2; bad[1].value = 2;
    assert(tunuino_analog_convert(&curve, 1, true, &value) == TUNUINO_ANALOG_CONFIGURATION);
    curve.minimum = 3;
    assert(tunuino_analog_convert(&curve, 1, true, &value) == TUNUINO_ANALOG_CONFIGURATION);
    curve.count = 1;
    assert(tunuino_analog_convert(&curve, 1, true, &value) == TUNUINO_ANALOG_CONFIGURATION);
    curve.count = 33;
    assert(tunuino_analog_convert(&curve, 1, true, &value) == TUNUINO_ANALOG_CONFIGURATION);
    curve.count = 2; curve.points = NULL;
    assert(tunuino_analog_convert(&curve, 1, true, &value) == TUNUINO_ANALOG_CONFIGURATION);
    tunuino_analog_point maximum[32];
    for (size_t i = 0; i < 32; ++i) maximum[i] = (tunuino_analog_point){(uint16_t)i, (int32_t)i};
    curve = (tunuino_analog_curve){maximum, 32, 0, 31};
    assert(tunuino_analog_convert(&curve, 31, true, &value) == TUNUINO_ANALOG_OK && value == 31);

    const tunuino_analog_point positive[] = {{100, 50000}, {300, 150000}};
    curve = (tunuino_analog_curve){positive, 2, 50000, 150000};
    tunuino_telemetry_sensors sensors = {0};
    assert(tunuino_analog_update(&sensors, TUNUINO_ANALOG_MAP, &curve, 200, true, 123) == TUNUINO_ANALOG_OK);
    assert(sensors.map_milli_kpa == 100000 && sensors.map_time_us == 123 && sensors.valid_mask == TUNUINO_LIVE_MAP_VALID);
    assert(tunuino_analog_update(&sensors, TUNUINO_ANALOG_LAMBDA, &curve, 300, true, 124) == TUNUINO_ANALOG_OK);
    assert(sensors.lambda_million == 150000 && sensors.lambda_time_us == 124);
    curve = (tunuino_analog_curve){points, 3, -10000, 60000};
    assert(tunuino_analog_update(&sensors, TUNUINO_ANALOG_COOLANT, &curve, 150, true, 125) == TUNUINO_ANALOG_OK);
    assert(sensors.coolant_milli_c == -5000 && sensors.coolant_time_us == 125 && sensors.valid_mask == TUNUINO_TELEMETRY_SENSOR_MASK);
    assert(tunuino_analog_update(&sensors, TUNUINO_ANALOG_MAP, &curve, 200, true, 126) == TUNUINO_ANALOG_CONFIGURATION);
    assert(sensors.map_milli_kpa == 0 && sensors.map_time_us == 126);
    assert(sensors.valid_mask == (TUNUINO_LIVE_LAMBDA_VALID | TUNUINO_LIVE_COOLANT_VALID));
    assert(tunuino_analog_update(&sensors, TUNUINO_ANALOG_COOLANT, &curve, 150, false, 127) == TUNUINO_ANALOG_INVALID_SAMPLE);
    assert(sensors.coolant_milli_c == 0 && sensors.coolant_time_us == 127 && sensors.valid_mask == TUNUINO_LIVE_LAMBDA_VALID);
    curve = (tunuino_analog_curve){positive, 2, 0, 150000};
    assert(tunuino_analog_update(&sensors, TUNUINO_ANALOG_LAMBDA, &curve, 200, true, 128) == TUNUINO_ANALOG_CONFIGURATION);
    assert(sensors.lambda_million == 0 && sensors.valid_mask == 0);
    curve.minimum = 50000;
    assert(tunuino_analog_update(&sensors, TUNUINO_ANALOG_MAP, &curve, 100, true, 129) == TUNUINO_ANALOG_OK);
    assert(tunuino_analog_update(&sensors, TUNUINO_ANALOG_MAP, &curve, 301, true, 130) == TUNUINO_ANALOG_OUT_OF_RANGE);
    assert(sensors.map_milli_kpa == 0 && sensors.valid_mask == 0);
    assert(tunuino_analog_update(&sensors, (tunuino_analog_channel)99, &curve, 100, true, 131) == TUNUINO_ANALOG_CONFIGURATION);
    assert(sensors.map_time_us == 130);
    assert(tunuino_analog_update(NULL, TUNUINO_ANALOG_MAP, &curve, 100, true, 0) == TUNUINO_ANALOG_CONFIGURATION);
    puts("C sensor calibration, integer limits, invalidation and telemetry snapshot checks passed");
    return 0;
}
