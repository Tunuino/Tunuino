// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "tune_maps.h"
#include <limits.h>
static uint8_t axis_valid(const uint16_t *axis) {
    for (unsigned i = 1u; i < TUNUINO_MAP_AXIS_SIZE; ++i)
        if (axis[i] <= axis[i - 1u]) return 0u;
    return 1u;
}
uint8_t tunuino_tune_map_valid(const tunuino_tune_map_t *map) {
    return map && axis_valid(map->rpm_axis) && axis_valid(map->load_axis);
}
static unsigned segment(const uint16_t *axis, uint16_t value) {
    unsigned i = 0u;
    while (i < TUNUINO_MAP_AXIS_SIZE - 2u && value > axis[i + 1u]) ++i;
    return i;
}
static int32_t interpolate(int32_t a, int32_t b,
    uint32_t offset, uint32_t span) {
    // 64-bit intermediate avoids overflow even for full int16 cell range.
    const int64_t numerator = (int64_t)(b - a) * (int64_t)offset;
    return a + (int32_t)(numerator / (int64_t)span);
}
uint8_t tunuino_tune_map_lookup(const tunuino_tune_map_t *map,
    uint16_t rpm, uint16_t load_tenths_kpa, int16_t *result) {
    if (!result || !tunuino_tune_map_valid(map)) return 0u;
    const unsigned x = segment(map->rpm_axis, rpm);
    const unsigned y = segment(map->load_axis, load_tenths_kpa);
    const uint32_t x0 = map->rpm_axis[x], x1 = map->rpm_axis[x + 1u];
    const uint32_t y0 = map->load_axis[y], y1 = map->load_axis[y + 1u];
    const uint32_t dx = rpm <= x0 ? 0u : rpm >= x1 ? x1 - x0 : rpm - x0;
    const uint32_t dy = load_tenths_kpa <= y0 ? 0u :
        load_tenths_kpa >= y1 ? y1 - y0 : load_tenths_kpa - y0;
    const int32_t a = interpolate(map->cells[y][x], map->cells[y][x + 1u], dx, x1 - x0);
    const int32_t b = interpolate(map->cells[y + 1u][x],
        map->cells[y + 1u][x + 1u], dx, x1 - x0);
    const int32_t value = interpolate(a, b, dy, y1 - y0);
    if (value < INT16_MIN || value > INT16_MAX) return 0u;
    *result = (int16_t)value;
    return 1u;
}
