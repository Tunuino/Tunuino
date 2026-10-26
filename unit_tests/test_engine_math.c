/* Copyright (c) 2026 Tunuino. Mathematical dry-run only. */
#include "tunuino/engine_math.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    tunuino_engine_math_result value={0};
    /* At 1,000 rpm: one revolution=60ms and one 720-degree cycle=120ms.
     * VE80% and MAP75kPa = a dimensionless air-load proxy of 60%,
     * not injection time or commanded fuel. */
    assert(tunuino_engine_math_calculate(1000000,36,1,8000,1200,75000,&value)
        ==TUNUINO_ENGINE_MATH_OK);
    assert(value.revolution_us==60000 && value.cycle_us==120000);
    assert(value.wheel_slot_us==1667 && value.load_centi_percent==6000);
    assert(value.spark_relative_us==-2000);
    tunuino_engine_math_result before=value;
    assert(tunuino_engine_math_calculate(1000000,36,1,8000,-500,75000,&value)
        ==TUNUINO_ENGINE_MATH_OK && value.spark_relative_us==833);
    assert(tunuino_engine_math_calculate(1000000,36,1,8000,0,75000,&value)
        ==TUNUINO_ENGINE_MATH_OK && value.spark_relative_us==0);
    before=value;
    assert(tunuino_engine_math_calculate(1000000,36,0,8000,0,75000,&value)
        ==TUNUINO_ENGINE_MATH_RANGE);
    assert(tunuino_engine_math_calculate(1000000,36,1,0,0,75000,&value)
        ==TUNUINO_ENGINE_MATH_RANGE);
    assert(tunuino_engine_math_calculate(0,36,1,8000,0,75000,&value)
        ==TUNUINO_ENGINE_MATH_RANGE);
    assert(tunuino_engine_math_calculate(1000000,36,1,8000,9001,75000,&value)
        ==TUNUINO_ENGINE_MATH_RANGE);
    assert(tunuino_engine_math_calculate(1000000,36,1,8000,0,500001,&value)
        ==TUNUINO_ENGINE_MATH_RANGE);
    assert(tunuino_engine_math_calculate(1000000,36,1,8000,0,75000,0)
        ==TUNUINO_ENGINE_MATH_ARGUMENT);
    assert(value.revolution_us==before.revolution_us && value.cycle_us==before.cycle_us
        && value.wheel_slot_us==before.wheel_slot_us
        && value.spark_relative_us==before.spark_relative_us);
    /* Maximum supported speed and pressure: no overflow. */
    assert(tunuino_engine_math_calculate(20000000,120,1,30000,-9000,500000,&value)
        ==TUNUINO_ENGINE_MATH_OK);
    assert(value.revolution_us==3000 && value.load_centi_percent==150000);
    puts("Dry-run crank timing, relative load, signed advance and invalid-input checks passed");
    return 0;
}
