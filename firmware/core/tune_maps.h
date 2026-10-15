// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TUNE_MAPS_H
#define TUNUINO_TUNE_MAPS_H
#include <stdint.h>
#define TUNUINO_MAP_AXIS_SIZE 16u
typedef struct {
    uint16_t rpm_axis[TUNUINO_MAP_AXIS_SIZE];
    uint16_t load_axis[TUNUINO_MAP_AXIS_SIZE]; // kPa x10
    int16_t cells[TUNUINO_MAP_AXIS_SIZE][TUNUINO_MAP_AXIS_SIZE];
} tunuino_tune_map_t;
typedef struct {
    tunuino_tune_map_t ve;       // VE x10
    tunuino_tune_map_t ignition; // degrees BTDC x10
} tunuino_tune_maps_t;
// Axes must be strictly increasing. Returns zero for invalid tables.
uint8_t tunuino_tune_map_valid(const tunuino_tune_map_t *map);
// Fixed-point bilinear interpolation with nearest-end clamping.
// Returns 0 for success; does not change output on failure.
uint8_t tunuino_tune_map_lookup(const tunuino_tune_map_t *map,
    uint16_t rpm, uint16_t load_tenths_kpa, int16_t *result);
#endif
