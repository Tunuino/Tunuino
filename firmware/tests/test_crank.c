/* Copyright (c) 2026 Tunuino. */
#include "tunuino/crank.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
static tunuino_crank crank;
static uint64_t now;
static tunuino_crank_config config;
static tunuino_crank_status edge(uint64_t duration) {
    now += duration;
    return tunuino_crank_edge(&crank,now);
}
static void invalid_read(void) {
    tunuino_crank_reading r = {1,123,456};
    assert(tunuino_crank_read(&crank,now,&r) != TUNUINO_CRANK_SYNCED);
    assert(r.valid == 0 && r.rpm == 0 && r.tooth_index == 0);
}
static void initialize(unsigned slots, unsigned missing) {
    config = (tunuino_crank_config){(uint16_t)slots,(uint8_t)missing,30,15000,60000000};
    now = 100;
    assert(tunuino_crank_init(&crank,&config,now) == TUNUINO_CRANK_ACQUIRING);
    assert(tunuino_crank_edge(&crank,now) == TUNUINO_CRANK_ACQUIRING);
}
static void lock(uint64_t period) {
    for (unsigned i = 0; i < 5; ++i) { assert(edge(period) == TUNUINO_CRANK_ACQUIRING); }
    assert(edge(period * (config.missing + 1u)) == TUNUINO_CRANK_ACQUIRING);
    invalid_read();
    for (unsigned i = 1; i < (unsigned)config.slots - config.missing; ++i) {
        assert(edge(period) == TUNUINO_CRANK_ACQUIRING);
    }
    assert(edge(period * (config.missing + 1u)) == TUNUINO_CRANK_SYNCED);
}
int main(void) {
    const unsigned wheels[][2] = {{12,1},{36,1},{60,2},{360,4}};
    const unsigned speeds[] = {60,300,1000,6000,12000};
    for (unsigned w = 0; w < sizeof(wheels) / sizeof(wheels[0]); ++w) {
        for (unsigned s = 0; s < sizeof(speeds) / sizeof(speeds[0]); ++s) {
            initialize(wheels[w][0],wheels[w][1]);
            uint64_t period = (uint64_t)llround(60000000.0 / (config.slots * speeds[s]));
            double expected = 60000000.0 / (config.slots * period);
            lock(period);
            for (unsigned revolution = 0; revolution < 3; ++revolution) {
                for (unsigned tooth = 0; tooth < (unsigned)config.slots - config.missing; ++tooth) {
                    tunuino_crank_reading r;
                    assert(tunuino_crank_read(&crank,now,&r) == TUNUINO_CRANK_SYNCED);
                    assert(r.valid && r.tooth_index == tooth && fabs(r.rpm - expected) < 1e-8);
                    if (tooth + 1 < (unsigned)config.slots - config.missing) { assert(edge(period) == TUNUINO_CRANK_SYNCED); }
                }
                /* Gap interval must not divide reported RPM by missing+1. */
                assert(edge(period * (config.missing + 1u)) == TUNUINO_CRANK_SYNCED);
            }
            assert(tunuino_crank_poll(&crank,now + config.timeout_us - 1) == TUNUINO_CRANK_SYNCED);
            now += config.timeout_us;
            tunuino_crank_reading r = {1,99,99};
            assert(tunuino_crank_read(&crank,now,&r) == TUNUINO_CRANK_LOST && !r.valid && r.rpm == 0);
            assert(tunuino_crank_edge(&crank,now) == TUNUINO_CRANK_ACQUIRING);
            lock(period);
        }
    }
    initialize(36,1); lock(1000);
    assert(edge(1) == TUNUINO_CRANK_LOST); invalid_read();
    lock(1000); /* Recovery requires fresh complete evidence. */
    for (unsigned i = 1; i < 35; ++i) { assert(edge(1000) == TUNUINO_CRANK_SYNCED); }
    assert(edge(1000) == TUNUINO_CRANK_LOST); invalid_read(); /* Expected gap is absent. */
    lock(1000);
    for (unsigned i = 0; i < 8; ++i) { assert(edge(1000) == TUNUINO_CRANK_SYNCED); }
    assert(edge(2000) == TUNUINO_CRANK_LOST); invalid_read(); /* Gap arrives early. */
    initialize(60,2);
    for (unsigned i = 0; i < 5; ++i) { edge(1000); }
    edge(3000);
    for (unsigned revolution = 0; revolution < 5; ++revolution) {
        for (unsigned i = 1; i < 57; ++i) { assert(edge(1000) != TUNUINO_CRANK_SYNCED); }
        assert(edge(3000) != TUNUINO_CRANK_SYNCED); invalid_read();
    }
    initialize(60,4);
    for (unsigned i = 0; i < 5; ++i) { edge(1000); }
    assert(edge(4000) == TUNUINO_CRANK_LOST); invalid_read(); /* Neighboring gap length is not accepted. */
    initialize(60,2); lock(1000);
    for (unsigned revolution = 0; revolution < 20; ++revolution) {
        for (unsigned i = 1; i < 58; ++i) { assert(edge(i % 2 ? 990 : 1010) == TUNUINO_CRANK_SYNCED); }
        assert(edge(3000) == TUNUINO_CRANK_SYNCED);
        tunuino_crank_reading r;
        assert(tunuino_crank_read(&crank,now,&r) == TUNUINO_CRANK_SYNCED && fabs(r.rpm - 1000) < 1e-8);
    }
    assert(tunuino_crank_edge(&crank,now) == TUNUINO_CRANK_LOST); invalid_read();
    assert(tunuino_crank_poll(&crank,now - 1) == TUNUINO_CRANK_CLOCK); invalid_read();
    assert(edge(1000) == TUNUINO_CRANK_CLOCK); /* Clock fault is sticky. */
    initialize(36,1); lock(1000);
    now += config.timeout_us;
    assert(tunuino_crank_edge(&crank,now) == TUNUINO_CRANK_LOST); invalid_read();
    initialize(36,1); lock(1000);
    assert(edge(1000000) == TUNUINO_CRANK_LOST); invalid_read(); /* Unexplained long interval. */
    config = (tunuino_crank_config){36,1,30,15000,60000000};
    now = UINT64_MAX - 1000000;
    assert(tunuino_crank_init(&crank,&config,now) == TUNUINO_CRANK_ACQUIRING);
    assert(tunuino_crank_edge(&crank,now) == TUNUINO_CRANK_ACQUIRING);
    lock(1000);
    assert(tunuino_crank_edge(&crank,0) == TUNUINO_CRANK_CLOCK);
    assert(tunuino_crank_init(NULL,&config,0) == TUNUINO_CRANK_ARGUMENT);
    assert(tunuino_crank_init(&crank,NULL,0) == TUNUINO_CRANK_ARGUMENT);
    assert(tunuino_crank_edge(&crank,0) == TUNUINO_CRANK_ARGUMENT);
    assert(tunuino_crank_poll(NULL,0) == TUNUINO_CRANK_ARGUMENT);
    tunuino_crank_reading r = {1,99,99};
    assert(tunuino_crank_read(NULL,0,&r) == TUNUINO_CRANK_ARGUMENT && !r.valid && r.rpm == 0);
    assert(tunuino_crank_read(&crank,0,NULL) == TUNUINO_CRANK_ARGUMENT);
    const tunuino_crank_config bad[] = {
        {11,1,30,15000,60000000},{361,1,30,15000,60000000},{36,0,30,15000,60000000},
        {36,5,30,15000,60000000},{36,1,NAN,15000,60000000},{36,1,30,INFINITY,60000000},
        {36,1,0,15000,60000000},{36,1,100,100,60000000},{36,1,30,30001,60000000},
        {360,1,30,30000,60000000},{36,1,30,15000,0},{36,1,30,15000,1},
        {36,1,30,15000,60000001}
    };
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        assert(tunuino_crank_init(&crank,&bad[i],0) == TUNUINO_CRANK_ARGUMENT);
        assert(tunuino_crank_edge(&crank,1) == TUNUINO_CRANK_ARGUMENT);
    }
    puts("C crank prototype: missing-tooth confirmation, 20 wheel/speed cases, phase, RPM, jitter, noise, timeout and reacquisition passed");
    return 0;
}
