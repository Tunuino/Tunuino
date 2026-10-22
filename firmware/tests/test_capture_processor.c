/* Copyright (c) 2026 Tunuino. */
#include "tunuino/capture_processor.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
static tunuino_capture_processor processor;
static tunuino_position_config config = {{36,1,30,15000,300000},9,11,0};
static uint64_t now, wall;
static tunuino_position_reading reading;
static tunuino_capture_process_status deliver(tunuino_capture_kind kind, uint64_t interval) {
    now += interval; if (now > wall) { wall = now; }
    assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,kind}) == TUNUINO_CAPTURE_OK);
    size_t count;
    tunuino_capture_process_status status = tunuino_capture_processor_drain(&processor,wall,16,&count);
    assert(count == 1);
    return status;
}
static void initialize(void) {
    now = wall = 100;
    assert(tunuino_capture_processor_init(&processor,&config,now) == TUNUINO_PROCESS_NO_SIGNAL);
    assert(deliver(TUNUINO_CAPTURE_CRANK,0) == TUNUINO_PROCESS_READY);
}
static void establish_crank(void) {
    for (unsigned i = 0; i < 5; ++i) { deliver(TUNUINO_CAPTURE_CRANK,1000); }
    deliver(TUNUINO_CAPTURE_CRANK,2000);
    for (unsigned i = 1; i < 35; ++i) { deliver(TUNUINO_CAPTURE_CRANK,1000); }
    assert(deliver(TUNUINO_CAPTURE_CRANK,2000) == TUNUINO_PROCESS_READY);
    assert(tunuino_capture_processor_read(&processor,wall,&reading) == TUNUINO_PROCESS_READY);
    assert(reading.crank_valid && !reading.cycle_valid && reading.tooth_index == 0);
}
static void advance(unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        assert(tunuino_capture_processor_read(&processor,wall,&reading) == TUNUINO_PROCESS_READY && reading.crank_valid);
        deliver(TUNUINO_CAPTURE_CRANK,reading.tooth_index == 34 ? 2000 : 1000);
    }
}
static void establish_cycle(void) {
    advance(10);
    deliver(TUNUINO_CAPTURE_CAM,0); advance(70); deliver(TUNUINO_CAPTURE_CAM,0);
    assert(tunuino_capture_processor_read(&processor,wall,&reading) == TUNUINO_PROCESS_READY && reading.cycle_valid);
}
static void zero_read(tunuino_capture_process_status expected, uint64_t time) {
    reading = (tunuino_position_reading){1,1,1,99,99};
    assert(tunuino_capture_processor_read(&processor,time,&reading) == expected);
    assert(!reading.crank_valid && !reading.cycle_valid && reading.rpm == 0 && reading.tooth_index == 0 && reading.revolution_phase == 0);
}
int main(void) {
    initialize(); establish_crank(); establish_cycle();
    /* A later freshness read cannot move the decoder past a delayed capture. */
    wall += 1500;
    assert(tunuino_capture_processor_read(&processor,wall,&reading) == TUNUINO_PROCESS_READY && reading.cycle_valid);
    assert(deliver(TUNUINO_CAPTURE_CRANK,1000) == TUNUINO_PROCESS_READY);
    assert(tunuino_capture_processor_read(&processor,wall,&reading) == TUNUINO_PROCESS_READY && reading.cycle_valid);
    /* Partial work hides the cached phase until every queued event is decoded. */
    now += 1000; assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    now += 1000; assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK); wall = now;
    zero_read(TUNUINO_PROCESS_BACKLOG,wall);
    size_t processed = 99;
    assert(tunuino_capture_processor_drain(&processor,wall,1,&processed) == TUNUINO_PROCESS_BACKLOG && processed == 1);
    zero_read(TUNUINO_PROCESS_BACKLOG,wall);
    assert(tunuino_capture_processor_drain(&processor,wall,1,&processed) == TUNUINO_PROCESS_READY && processed == 1);
    assert(tunuino_capture_processor_read(&processor,wall,&reading) == TUNUINO_PROCESS_READY && reading.cycle_valid);
    /* Producer captures after the consumer sampled wall time: defer, do not lose it. */
    now += 1000; assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_processor_drain(&processor,wall,8,&processed) == TUNUINO_PROCESS_FUTURE && processed == 0);
    zero_read(TUNUINO_PROCESS_FUTURE,wall);
    now += 1000; assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    wall += 1000;
    assert(tunuino_capture_processor_drain(&processor,wall,1,&processed) == TUNUINO_PROCESS_BACKLOG && processed == 1);
    assert(tunuino_capture_processor_drain(&processor,wall,1,&processed) == TUNUINO_PROCESS_FUTURE && processed == 0);
    wall = now;
    assert(tunuino_capture_processor_drain(&processor,wall,1,&processed) == TUNUINO_PROCESS_READY && processed == 1);
    assert(tunuino_capture_processor_read(&processor,wall,&reading) == TUNUINO_PROCESS_READY && reading.cycle_valid);
    /* Expiry invalidates phase, but a subsequently published old capture is not a clock rollback. */
    wall = now + config.crank.timeout_us;
    zero_read(TUNUINO_PROCESS_NO_SIGNAL,wall);
    wall += 5000;
    assert(deliver(TUNUINO_CAPTURE_CRANK,1000) == TUNUINO_PROCESS_NO_SIGNAL);
    zero_read(TUNUINO_PROCESS_NO_SIGNAL,wall);
    now = wall;
    assert(deliver(TUNUINO_CAPTURE_CRANK,1000) == TUNUINO_PROCESS_READY);
    assert(tunuino_capture_processor_read(&processor,wall,&reading) == TUNUINO_PROCESS_READY && !reading.cycle_valid);
    establish_crank(); establish_cycle();
    /* Overflow after valid synchronization hides every cached field and latches. */
    for (unsigned i = 0; i < TUNUINO_CAPTURE_CAPACITY; ++i) {
        now += 1000;
        assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    }
    now += 1000;
    assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OVERFLOW);
    wall = now; zero_read(TUNUINO_PROCESS_QUEUE_FAULT,wall);
    assert(processor.queue_status == TUNUINO_CAPTURE_OVERFLOW);
    assert(tunuino_capture_processor_drain(&processor,wall,64,&processed) == TUNUINO_PROCESS_QUEUE_FAULT && processed == 0);
    zero_read(TUNUINO_PROCESS_QUEUE_FAULT,wall + 1);
    initialize(); establish_crank(); establish_cycle();
    assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now - 1,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_ORDER);
    zero_read(TUNUINO_PROCESS_QUEUE_FAULT,wall);
    initialize(); establish_crank(); establish_cycle();
    zero_read(TUNUINO_PROCESS_CLOCK,wall - 1); zero_read(TUNUINO_PROCESS_CLOCK,wall + 1);
    assert(tunuino_capture_processor_drain(&processor,wall + 2,8,&processed) == TUNUINO_PROCESS_CLOCK && processed == 0);
    /* A source timestamp older than initialization is a decoder clock fault. */
    initialize();
    assert(tunuino_capture_processor_init(&processor,&config,1000) == TUNUINO_PROCESS_NO_SIGNAL);
    assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){999,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_processor_drain(&processor,1000,8,&processed) == TUNUINO_PROCESS_CLOCK && processed == 1);
    zero_read(TUNUINO_PROCESS_CLOCK,1000);
    /* Cam-only traffic cannot satisfy the crank freshness condition. */
    assert(tunuino_capture_processor_init(&processor,&config,0) == TUNUINO_PROCESS_NO_SIGNAL);
    assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){1,TUNUINO_CAPTURE_CAM}) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_processor_drain(&processor,1,8,&processed) == TUNUINO_PROCESS_NO_SIGNAL && processed == 1);
    zero_read(TUNUINO_PROCESS_NO_SIGNAL,1);
    assert(tunuino_capture_processor_init(NULL,&config,0) == TUNUINO_PROCESS_ARGUMENT);
    assert(tunuino_capture_processor_init(&processor,NULL,0) == TUNUINO_PROCESS_ARGUMENT);
    zero_read(TUNUINO_PROCESS_ARGUMENT,0);
    assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){1,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_ARGUMENT);
    assert(tunuino_capture_processor_push(NULL,(tunuino_capture_event){1,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_ARGUMENT);
    assert(tunuino_capture_processor_drain(NULL,0,1,&processed) == TUNUINO_PROCESS_ARGUMENT && processed == 0);
    assert(tunuino_capture_processor_drain(&processor,0,0,&processed) == TUNUINO_PROCESS_ARGUMENT);
    assert(tunuino_capture_processor_drain(&processor,0,65,&processed) == TUNUINO_PROCESS_ARGUMENT);
    assert(tunuino_capture_processor_drain(&processor,0,1,NULL) == TUNUINO_PROCESS_ARGUMENT);
    assert(tunuino_capture_processor_read(NULL,0,&reading) == TUNUINO_PROCESS_ARGUMENT && !reading.crank_valid);
    assert(tunuino_capture_processor_read(&processor,0,NULL) == TUNUINO_PROCESS_ARGUMENT);
    tunuino_position_config bad = config; bad.cam_revolution_phase = 2;
    assert(tunuino_capture_processor_init(&processor,&bad,0) == TUNUINO_PROCESS_ARGUMENT);
    puts("C capture processor: ordered decoding, bounded work, deferred events, delayed publication, stale phase, queue faults and recovery passed");
    return 0;
}
