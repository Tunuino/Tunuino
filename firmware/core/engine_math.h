// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_ENGINE_MATH_H
#define TUNUINO_ENGINE_MATH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// All calculations are pure and have no hardware side effects.
typedef struct {
    float rpm;
    float map_kpa;
    float tps_percent;
    float coolant_c;
    float intake_c;
} tunuino_engine_inputs_t;
float tunuino_clampf(float value, float low, float high);
float tunuino_interpolate(float x, float x0, float x1, float y0, float y1);
// Returns 0 when RPM is invalid or engine stopped.
float tunuino_cycle_ms(float rpm, uint8_t four_stroke);
#ifdef __cplusplus
}
#endif
#endif
