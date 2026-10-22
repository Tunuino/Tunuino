/* Copyright (c) 2026 Tunuino. */
#include "tunuino/position.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
static tunuino_position position;
static tunuino_position_config config;
static uint64_t now;
static tunuino_position_status crank_edge(uint64_t duration) {
    now += duration;
    return tunuino_position_crank_edge(&position,now);
}
static tunuino_position_reading read_position(void) {
    tunuino_position_reading reading;
    (void)tunuino_position_read(&position,now,&reading);
    return reading;
}
static void establish_crank(void) {
    for (unsigned i = 0; i < 5; ++i) { crank_edge(1000); }
    crank_edge(1000u * (config.crank.missing + 1u));
    for (unsigned i = 1; i < (unsigned)config.crank.slots - config.crank.missing; ++i) { crank_edge(1000); }
    assert(crank_edge(1000u * (config.crank.missing + 1u)) == TUNUINO_POSITION_CRANK_ONLY);
    tunuino_position_reading r = read_position();
    assert(r.crank_valid && !r.cycle_valid && r.tooth_index == 0 && r.revolution_phase == 0);
}
static void initialize(unsigned slots, unsigned missing, unsigned phase) {
    config = (tunuino_position_config){{(uint16_t)slots,(uint8_t)missing,30,15000,300000},9,11,(uint8_t)phase};
    now = 100;
    assert(tunuino_position_init(&position,&config,now) == TUNUINO_POSITION_ACQUIRING);
    assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_ACQUIRING);
    assert(tunuino_position_crank_edge(&position,now) == TUNUINO_POSITION_ACQUIRING);
    establish_crank();
}
static void advance(unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        tunuino_position_reading r = read_position();
        assert(r.crank_valid);
        uint64_t interval = r.tooth_index + 1u == (unsigned)config.crank.slots - config.crank.missing
            ? 1000u * (config.crank.missing + 1u) : 1000;
        tunuino_position_status status = crank_edge(interval);
        assert(status == TUNUINO_POSITION_CRANK_ONLY || status == TUNUINO_POSITION_SYNCED || status == TUNUINO_POSITION_CAM_LOST);
    }
}
static void at_tooth(unsigned tooth) {
    for (unsigned i = 0; read_position().tooth_index != tooth; ++i) {
        assert(i < config.crank.slots); advance(1);
    }
}
static void confirm_cam(void) {
    at_tooth(10);
    assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_CRANK_ONLY);
    assert(!read_position().cycle_valid);
    advance(2u * (config.crank.slots - config.crank.missing));
    assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_SYNCED);
    tunuino_position_reading r = read_position();
    assert(r.crank_valid && r.cycle_valid && r.revolution_phase == config.cam_revolution_phase);
}
int main(void) {
    const unsigned wheels[][2] = {{36,1},{60,2}};
    for (unsigned w = 0; w < 2; ++w) {
        for (unsigned phase = 0; phase <= 1; ++phase) {
            initialize(wheels[w][0],wheels[w][1],phase); confirm_cam();
            for (unsigned cycle = 0; cycle < 10; ++cycle) {
                advance(config.crank.slots - config.crank.missing);
                tunuino_position_reading r = read_position();
                assert(r.cycle_valid && r.revolution_phase == (phase ^ 1u));
                advance(config.crank.slots - config.crank.missing);
                r = read_position(); assert(r.cycle_valid && r.revolution_phase == phase);
                assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_SYNCED);
                double rpm = 60000000.0 / (config.crank.slots * 1000u);
                assert(fabs(read_position().rpm - rpm) < 1e-8);
            }
            /* Missing expected cam invalidates the cycle when its window ends. */
            advance(2u * (config.crank.slots - config.crank.missing) + 2u);
            tunuino_position_reading r = read_position();
            assert(r.crank_valid && !r.cycle_valid && r.rpm > 0 && r.revolution_phase == 0);
            assert(tunuino_position_poll(&position,now) == TUNUINO_POSITION_CAM_LOST);
            confirm_cam();
            /* A one-revolution cam period contradicts the configured 720 model. */
            advance(config.crank.slots - config.crank.missing);
            assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_CAM_LOST);
            assert(read_position().crank_valid && !read_position().cycle_valid);
        }
    }
    initialize(60,2,0); confirm_cam();
    assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_CAM_LOST); /* Duplicate. */
    assert(!read_position().cycle_valid);
    confirm_cam(); at_tooth(20);
    assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_CAM_LOST); /* Wrong window. */
    assert(read_position().crank_valid && !read_position().cycle_valid);
    confirm_cam();
    assert(crank_edge(1) == TUNUINO_POSITION_CRANK_LOST);
    tunuino_position_reading r = read_position();
    assert(!r.crank_valid && !r.cycle_valid && r.rpm == 0 && r.tooth_index == 0 && r.revolution_phase == 0);
    establish_crank(); /* Prior cam phase cannot survive crank reacquisition. */
    assert(!read_position().cycle_valid); confirm_cam();
    now += config.crank.timeout_us;
    assert(tunuino_position_read(&position,now,&r) == TUNUINO_POSITION_CRANK_LOST && !r.crank_valid && !r.cycle_valid && r.rpm == 0);
    assert(crank_edge(1000) == TUNUINO_POSITION_ACQUIRING);
    establish_crank(); confirm_cam();
    assert(tunuino_position_cam_edge(&position,now - 1) == TUNUINO_POSITION_CLOCK);
    assert(tunuino_position_read(&position,now,&r) == TUNUINO_POSITION_CLOCK && !r.crank_valid && !r.cycle_valid);
    assert(crank_edge(1000) == TUNUINO_POSITION_CLOCK);
    /* Polling a gap repeatedly must not count revolutions. Boundary window 0. */
    initialize(36,1,1);
    config.cam_window_first = config.cam_window_last = 0;
    assert(tunuino_position_init(&position,&config,now) == TUNUINO_POSITION_ACQUIRING);
    assert(tunuino_position_crank_edge(&position,now) == TUNUINO_POSITION_ACQUIRING); establish_crank();
    assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_CRANK_ONLY);
    for (unsigned i = 0; i < 100; ++i) { assert(tunuino_position_poll(&position,now) == TUNUINO_POSITION_CRANK_ONLY); }
    advance(70); /* Two physical revolutions of 36-1. */
    assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_SYNCED);
    assert(read_position().revolution_phase == 1);
    /* Last physical tooth as a one-tooth window; expiry occurs at next gap. */
    initialize(36,1,0); config.cam_window_first = config.cam_window_last = 34;
    assert(tunuino_position_init(&position,&config,now) == TUNUINO_POSITION_ACQUIRING);
    tunuino_position_crank_edge(&position,now); establish_crank(); at_tooth(34);
    assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_CRANK_ONLY);
    advance(70); assert(tunuino_position_cam_edge(&position,now) == TUNUINO_POSITION_SYNCED);
    advance(71); assert(read_position().crank_valid && !read_position().cycle_valid);
    assert(tunuino_position_init(NULL,&config,0) == TUNUINO_POSITION_ARGUMENT);
    assert(tunuino_position_init(&position,NULL,0) == TUNUINO_POSITION_ARGUMENT);
    assert(tunuino_position_cam_edge(&position,0) == TUNUINO_POSITION_ARGUMENT);
    assert(tunuino_position_crank_edge(NULL,0) == TUNUINO_POSITION_ARGUMENT);
    assert(tunuino_position_cam_edge(NULL,0) == TUNUINO_POSITION_ARGUMENT);
    assert(tunuino_position_poll(NULL,0) == TUNUINO_POSITION_ARGUMENT);
    r = (tunuino_position_reading){1,1,1,99,99};
    assert(tunuino_position_read(NULL,0,&r) == TUNUINO_POSITION_ARGUMENT && !r.crank_valid && !r.cycle_valid && r.rpm == 0);
    assert(tunuino_position_read(&position,0,NULL) == TUNUINO_POSITION_ARGUMENT);
    tunuino_position_config bad = config; bad.cam_window_first = 11; bad.cam_window_last = 10;
    assert(tunuino_position_init(&position,&bad,0) == TUNUINO_POSITION_ARGUMENT);
    bad = config; bad.cam_window_last = 35;
    assert(tunuino_position_init(&position,&bad,0) == TUNUINO_POSITION_ARGUMENT);
    bad = config; bad.cam_revolution_phase = 2;
    assert(tunuino_position_init(&position,&bad,0) == TUNUINO_POSITION_ARGUMENT);
    bad = config; bad.crank.slots = 1;
    assert(tunuino_position_init(&position,&bad,0) == TUNUINO_POSITION_ARGUMENT);
    puts("C 720-degree prototype: two cam confirmations, phase parity, missing/extra cam, window limits, crank loss and reacquisition passed");
    return 0;
}
