/* Copyright (c) 2026 Tunuino. */
#include "tunuino/clock32.h"
#include "tunuino/capture_processor.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

static tunuino_clock32 clock;
static tunuino_capture_processor captures;
static uint64_t source_us;
static uint32_t latency_us;
static uint64_t wall_us;
static void edge(uint32_t interval) {
    source_us += interval;
    uint64_t time;
    const uint32_t delay = latency_us ? (uint32_t)((source_us / 1000u) % 3u) * 100u : 0;
    assert(tunuino_clock32_observe(&clock,(uint32_t)(source_us + delay),&wall_us) == TUNUINO_CLOCK32_OK);
    assert(wall_us == source_us + delay);
    assert(tunuino_clock32_capture(&clock,(uint32_t)source_us,1000,&time) == TUNUINO_CLOCK32_OK && time == source_us);
    assert(tunuino_capture_processor_push(&captures,(tunuino_capture_event){time,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    size_t count;
    assert(tunuino_capture_processor_drain(&captures,wall_us,64,&count) == TUNUINO_PROCESS_READY && count == 1);
}

int main(void) {
    uint64_t time = 123;
    assert(tunuino_clock32_observe(&clock,0,&time) == TUNUINO_CLOCK32_ARGUMENT && time == 0);
    assert(tunuino_clock32_init(NULL,0,0,1) == TUNUINO_CLOCK32_ARGUMENT);
    assert(tunuino_clock32_init(&clock,0,0,0) == TUNUINO_CLOCK32_ARGUMENT);
    assert(tunuino_clock32_init(&clock,0,0,UINT32_MAX) == TUNUINO_CLOCK32_ARGUMENT);
    assert(tunuino_clock32_observe(&clock,0,&time) == TUNUINO_CLOCK32_ARGUMENT);
    assert(tunuino_clock32_observe(NULL,0,&time) == TUNUINO_CLOCK32_ARGUMENT && time == 0);
    assert(tunuino_clock32_capture(NULL,0,1,&time) == TUNUINO_CLOCK32_ARGUMENT && time == 0);
    assert(tunuino_clock32_capture(&clock,0,1,&time) == TUNUINO_CLOCK32_ARGUMENT && time == 0);
    assert(tunuino_clock32_init(&clock,UINT32_MAX - 99u,5000,1000) == TUNUINO_CLOCK32_OK);
    assert(tunuino_clock32_capture(&clock,UINT32_MAX - 100u,1000,&time) == TUNUINO_CLOCK32_REJECTED && time == 0);
    assert(tunuino_clock32_observe(&clock,100,&time) == TUNUINO_CLOCK32_OK && time == 5200);
    assert(tunuino_clock32_capture(&clock,UINT32_MAX - 49u,150,&time) == TUNUINO_CLOCK32_OK && time == 5050);
    assert(tunuino_clock32_capture(&clock,UINT32_MAX - 49u,149,&time) == TUNUINO_CLOCK32_REJECTED && time == 0);
    assert(tunuino_clock32_capture(&clock,101,1000,&time) == TUNUINO_CLOCK32_REJECTED && time == 0);
    assert(tunuino_clock32_capture(&clock,100,1,&time) == TUNUINO_CLOCK32_OK && time == 5200);
    assert(tunuino_clock32_capture(&clock,100,0,&time) == TUNUINO_CLOCK32_ARGUMENT && time == 0);
    assert(tunuino_clock32_capture(&clock,100,UINT32_MAX,&time) == TUNUINO_CLOCK32_ARGUMENT && time == 0);
    assert(tunuino_clock32_capture(&clock,100,1,NULL) == TUNUINO_CLOCK32_ARGUMENT);
    assert(tunuino_clock32_observe(&clock,100,&time) == TUNUINO_CLOCK32_OK && time == 5200);
    assert(tunuino_clock32_observe(&clock,99,&time) == TUNUINO_CLOCK32_FAULT);
    assert(tunuino_clock32_capture(&clock,99,1,&time) == TUNUINO_CLOCK32_FAULT && time == 0);
    assert(tunuino_clock32_init(&clock,UINT32_MAX - 9u,100,20) == TUNUINO_CLOCK32_OK);
    assert(tunuino_clock32_observe(&clock,UINT32_MAX - 9u,&time) == TUNUINO_CLOCK32_OK && time == 100);
    assert(tunuino_clock32_observe(&clock,0,&time) == TUNUINO_CLOCK32_OK && time == 110);
    assert(tunuino_clock32_observe(&clock,20,&time) == TUNUINO_CLOCK32_OK && time == 130);
    assert(tunuino_clock32_observe(&clock,21,NULL) == TUNUINO_CLOCK32_ARGUMENT);
    assert(tunuino_clock32_observe(&clock,20,&time) == TUNUINO_CLOCK32_OK && time == 130);
    assert(tunuino_clock32_observe(&clock,41,&time) == TUNUINO_CLOCK32_FAULT && time == 0);
    assert(tunuino_clock32_observe(&clock,21,&time) == TUNUINO_CLOCK32_FAULT && time == 0);
    assert(tunuino_clock32_init(&clock,100,0,1000) == TUNUINO_CLOCK32_OK);
    assert(tunuino_clock32_observe(&clock,99,&time) == TUNUINO_CLOCK32_FAULT && time == 0);
    assert(tunuino_clock32_init(&clock,0,0,INT32_MAX) == TUNUINO_CLOCK32_OK);
    assert(tunuino_clock32_observe(&clock,INT32_MAX,&time) == TUNUINO_CLOCK32_OK && time == INT32_MAX);
    assert(tunuino_clock32_observe(&clock,UINT32_MAX,&time) == TUNUINO_CLOCK32_FAULT && time == 0);
    assert(tunuino_clock32_init(&clock,0,UINT64_MAX - 1u,1) == TUNUINO_CLOCK32_OK);
    assert(tunuino_clock32_observe(&clock,1,&time) == TUNUINO_CLOCK32_OK && time == UINT64_MAX);
    assert(tunuino_clock32_capture(&clock,0,1,&time) == TUNUINO_CLOCK32_OK && time == UINT64_MAX - 1u);
    assert(tunuino_clock32_observe(&clock,1,&time) == TUNUINO_CLOCK32_OK && time == UINT64_MAX);
    assert(tunuino_clock32_observe(&clock,2,&time) == TUNUINO_CLOCK32_FAULT && time == 0);
    /* Multiple wraps with a deterministic independent 64-bit time reference. */
    uint64_t expected = UINT32_MAX - 123u;
    assert(tunuino_clock32_init(&clock,(uint32_t)expected,expected,1000000) == TUNUINO_CLOCK32_OK);
    uint32_t random = 12345;
    for (unsigned i = 0; i < 20000; ++i) {
        random = random * 1664525u + 1013904223u;
        expected += random % 1000001u;
        assert(tunuino_clock32_observe(&clock,(uint32_t)expected,&time) == TUNUINO_CLOCK32_OK && time == expected);
        uint32_t age = random % 1001u;
        assert(tunuino_clock32_capture(&clock,(uint32_t)(expected - age),1000,&time) == TUNUINO_CLOCK32_OK && time == expected - age);
    }
    assert(expected > (uint64_t)UINT32_MAX * 3u);
    const uint32_t until_wrap[] = {100,1000,42000,50000};
    const tunuino_position_config config = {{36,1,30,15000,300000},9,11,0};
    for (size_t scenario = 0; scenario < 8; ++scenario) {
        latency_us = scenario < 4 ? 0 : 200;
        source_us = (uint64_t)UINT32_MAX - until_wrap[scenario % 4];
        assert(tunuino_clock32_init(&clock,(uint32_t)source_us,source_us,1000000) == TUNUINO_CLOCK32_OK);
        assert(tunuino_capture_processor_init(&captures,&config,source_us) == TUNUINO_PROCESS_NO_SIGNAL);
        edge(0);
        for (unsigned i = 0; i < 5; ++i) edge(1000);
        edge(2000);
        for (unsigned i = 1; i < 35; ++i) edge(1000);
        edge(2000);
        tunuino_position_reading position;
        for (unsigned i = 0; i < 200; ++i) {
            assert(tunuino_capture_processor_read(&captures,wall_us,&position) == TUNUINO_PROCESS_READY);
            assert(position.crank_valid && !position.cycle_valid && fabs(position.rpm - 1666.6666666667) < 1e-6);
            edge(position.tooth_index == 34 ? 2000 : 1000);
        }
        assert(source_us > UINT32_MAX);
        source_us += config.crank.timeout_us;
        assert(tunuino_clock32_observe(&clock,(uint32_t)source_us,&time) == TUNUINO_CLOCK32_OK && time == source_us);
        assert(tunuino_capture_processor_read(&captures,time,&position) == TUNUINO_PROCESS_NO_SIGNAL && !position.crank_valid);
    }
    puts("C 32-bit clock: wraps, delayed captures, age/origin rejection, sticky faults and crank continuity passed");
    return 0;
}
