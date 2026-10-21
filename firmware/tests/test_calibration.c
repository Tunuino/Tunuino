/* Copyright (c) 2026 Tunuino. */
#include "tunuino/calibration.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* Test surface only; not an engine calibration. */
    tunuino_map map = {.kind = TUNUINO_MAP_VE, .rpm_count = 2, .load_count = 2,
        .rpm = {1000, 3000}, .load_milli_kpa = {50000, 100000},
        .cells = {{40000, 60000}, {80000, 100000}}};
    double value;
    assert(tunuino_map_lookup(&map, 2000, 75, &value) == TUNUINO_MAP_OK && fabs(value - 70) < 1e-9);
    map.cells[1][1] = 120000;
    assert(tunuino_map_lookup(&map, 2000, 75, &value) == TUNUINO_MAP_OK && value == 75);
    map.cells[1][1] = 100000;
    assert(tunuino_map_lookup(&map, 1000, 50, &value) == TUNUINO_MAP_OK && value == 40);
    assert(tunuino_map_lookup(&map, 3000, 100, &value) == TUNUINO_MAP_OK && value == 100);
    assert(tunuino_map_lookup(&map, 99999, 200, &value) == TUNUINO_MAP_CLAMPED && value == 100);
    assert(tunuino_map_lookup(&map, NAN, 100, &value) == TUNUINO_MAP_ARGUMENT);
    assert(tunuino_map_lookup(&map, 1000, INFINITY, &value) == TUNUINO_MAP_ARGUMENT);
    assert(tunuino_map_lookup(&map, -1, 100, &value) == TUNUINO_MAP_ARGUMENT);
    uint8_t bytes[TUNUINO_MAP_FILE_MAX];
    size_t length;
    assert(tunuino_map_encode(&map, bytes, sizeof(bytes), &length) == TUNUINO_MAP_OK && length == 48);
    for (size_t i = 0; i < length; ++i) { printf("%02x", bytes[i]); }
    puts("");
    tunuino_map decoded;
    memset(&decoded, 0, sizeof(decoded));
    assert(tunuino_map_decode(bytes, length, &decoded) == TUNUINO_MAP_OK);
    assert(tunuino_map_lookup(&decoded, 2000, 75, &value) == TUNUINO_MAP_OK && value == 70);
    uint8_t previous[sizeof(decoded)];
    memcpy(previous, &decoded, sizeof(decoded));
    for (size_t i = 0; i < length; ++i) {
        bytes[i] ^= 1;
        assert(tunuino_map_decode(bytes, length, &decoded) != TUNUINO_MAP_OK);
        assert(memcmp(&decoded, previous, sizeof(decoded)) == 0);
        bytes[i] ^= 1;
    }
    for (size_t n = 0; n < length; ++n) { assert(tunuino_map_decode(bytes, n, &decoded) != TUNUINO_MAP_OK); }
    memcpy(bytes + 16, bytes + 12, 4);
    uint32_t crc = tunuino_crc32(bytes, length - 4);
    for (unsigned i = 0; i < 4; ++i) { bytes[length - 4 + i] = (uint8_t)(crc >> (24u - i * 8u)); }
    assert(tunuino_map_decode(bytes, length, &decoded) == TUNUINO_MAP_RANGE);
    assert(memcmp(&decoded, previous, sizeof(decoded)) == 0);
    assert(tunuino_map_encode(&map, bytes, 47, &length) == TUNUINO_MAP_CAPACITY && length == 0);
    map.rpm[1] = map.rpm[0];
    assert(tunuino_map_validate(&map) == TUNUINO_MAP_RANGE);
    map.rpm[1] = 3000;
    map.kind = TUNUINO_MAP_IGNITION;
    map.cells[0][0] = -10000; map.cells[0][1] = 10000;
    map.cells[1][0] = 20000; map.cells[1][1] = 40000;
    assert(tunuino_map_lookup(&map, 2000, 75, &value) == TUNUINO_MAP_OK && value == 15);
    assert(tunuino_map_encode(&map, bytes, sizeof(bytes), &length) == TUNUINO_MAP_OK);
    for (size_t i = 0; i < length; ++i) { printf("%02x", bytes[i]); }
    puts("");
    assert(tunuino_map_decode(bytes, length, &decoded) == TUNUINO_MAP_OK && decoded.cells[0][0] == -10000);
    map.cells[0][0] = -90001;
    assert(tunuino_map_validate(&map) == TUNUINO_MAP_RANGE);
    map.kind = TUNUINO_MAP_VE; map.rpm_count = 32; map.load_count = 32;
    for (unsigned j = 0; j < 32; ++j) {
        map.rpm[j] = (int32_t)j * 100; map.load_milli_kpa[j] = (int32_t)j * 1000;
        for (unsigned i = 0; i < 32; ++i) { map.cells[j][i] = 50000; }
    }
    assert(tunuino_map_encode(&map, bytes, sizeof(bytes), &length) == TUNUINO_MAP_OK && length == sizeof(bytes));
    assert(tunuino_map_decode(bytes, length, &decoded) == TUNUINO_MAP_OK);
    assert(tunuino_map_lookup(&decoded, 2000, 20, &value) == TUNUINO_MAP_OK && value == 50);
    map.rpm_count = map.load_count = 3;
    map.rpm[0] = 0; map.rpm[1] = 1000; map.rpm[2] = 5000;
    map.load_milli_kpa[0] = 10000; map.load_milli_kpa[1] = 50000; map.load_milli_kpa[2] = 250000;
    for (unsigned j = 0; j < 3; ++j) {
        for (unsigned i = 0; i < 3; ++i) {
            map.cells[j][i] = 10000 + 10 * map.rpm[i] + map.load_milli_kpa[j] / 5 * 4;
        }
    }
    assert(tunuino_map_lookup(&map, 3000, 150, &value) == TUNUINO_MAP_OK && value == 160);
    puts("C calibration interpolation, bounds, CRC and atomic decode checks passed");
    return 0;
}
