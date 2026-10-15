// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/engine_math.h"
#include <assert.h>
#include <math.h>
int main(void) {
    assert(tunuino_clampf(150.0f, 0.0f, 100.0f) == 100.0f);
    assert(tunuino_clampf(-5.0f, 0.0f, 100.0f) == 0.0f);
    assert(tunuino_clampf(42.0f, 0.0f, 100.0f) == 42.0f);
    assert(isnan(tunuino_clampf(NAN, 0.0f, 100.0f)));
    assert(tunuino_interpolate(5.0f, 0.0f, 10.0f, 0.0f, 100.0f) == 50.0f);
    assert(isnan(tunuino_interpolate(1.0f, 2.0f, 2.0f, 0.0f, 100.0f)));
    assert(fabsf(tunuino_cycle_ms(3000.0f, 1) - 40.0f) < 0.001f);
    assert(fabsf(tunuino_cycle_ms(3000.0f, 0) - 20.0f) < 0.001f);
    assert(tunuino_cycle_ms(0.0f, 1) == 0.0f);
    return 0;
}
