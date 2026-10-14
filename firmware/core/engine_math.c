// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "engine_math.h"
#include <math.h>
float tunuino_clampf(float value, float low, float high) {
    if (!isfinite(value) || !isfinite(low) || !isfinite(high) || low > high) return NAN;
    if (value < low) return low;
    if (value > high) return high;
    return value;
}
float tunuino_interpolate(float x, float x0, float x1, float y0, float y1) {
    if (!isfinite(x) || !isfinite(x0) || !isfinite(x1) || !isfinite(y0) || !isfinite(y1) || x0 == x1) return NAN;
    float t = (x - x0) / (x1 - x0);
    return y0 + t * (y1 - y0);
}
float tunuino_cycle_ms(float rpm, uint8_t four_stroke) {
    if (!isfinite(rpm) || rpm <= 0.0f) return 0.0f;
    return (four_stroke ? 120000.0f : 60000.0f) / rpm;
}
