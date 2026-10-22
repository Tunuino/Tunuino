/* Copyright (c) 2026 Tunuino. */
#include "tunuino/capture_source.h"
#include "tunuino/telemetry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static tunuino_capture_source source;
static const tunuino_position_config config = {{36,1,30,15000,300000},9,11,0};
static uint64_t event_us, now_us;
static void edge(uint32_t interval) {
    event_us += interval;
    assert(tunuino_capture_source_observe(&source,(uint32_t)(event_us + 200),&now_us) == TUNUINO_SOURCE_OK);
    assert(now_us == event_us + 200);
    assert(tunuino_capture_source_push(&source,(uint32_t)event_us,TUNUINO_CAPTURE_CRANK) == TUNUINO_SOURCE_OK);
    size_t count;
    assert(tunuino_capture_processor_drain(&source.processor,now_us,64,&count) == TUNUINO_PROCESS_READY && count == 1);
}
static void initialize(void) {
    event_us = (uint64_t)UINT32_MAX - 100;
    assert(tunuino_capture_source_init(&source,&config,(uint32_t)event_us,event_us,10000,500) == TUNUINO_SOURCE_OK);
    edge(0);
    for (unsigned i = 0; i < 5; ++i) edge(1000);
    edge(2000);
    for (unsigned i = 1; i < 35; ++i) edge(1000);
    edge(2000);
    tunuino_position_reading reading;
    assert(tunuino_capture_processor_read(&source.processor,now_us,&reading) == TUNUINO_PROCESS_READY);
    assert(reading.crank_valid && fabs(reading.rpm - 1666.6666666667) < 1e-6);
}
static void no_rpm(tunuino_capture_status queue_fault) {
    assert(tunuino_capture_state(&source.processor.queue) == queue_fault);
    tunuino_position_reading reading;
    assert(tunuino_capture_processor_read(&source.processor,now_us,&reading) == TUNUINO_PROCESS_QUEUE_FAULT);
    assert(!reading.crank_valid && !reading.cycle_valid && reading.rpm == 0);
    tunuino_telemetry_sensors sensors = {now_us,now_us,now_us,100000,1000000,90000,TUNUINO_TELEMETRY_SENSOR_MASK};
    const tunuino_telemetry_policy policy = {10000,10000,10000};
    tunuino_live_sample live;
    assert(tunuino_telemetry_build(&source.processor,&sensors,&policy,now_us,&live) == TUNUINO_TELEMETRY_NOT_READY);
    assert(live.rpm_milli == 0 && live.valid_mask == TUNUINO_TELEMETRY_SENSOR_MASK);
    size_t count = 123;
    assert(tunuino_capture_processor_drain(&source.processor,now_us,64,&count) == TUNUINO_PROCESS_QUEUE_FAULT && count == 0);
    tunuino_capture_event event = {1,TUNUINO_CAPTURE_CRANK};
    assert(tunuino_capture_take(&source.processor.queue,&event) == queue_fault && event.time_us == 0 && event.kind == 0);
}
static void batch_checks(void) {
    const uint64_t origin = (uint64_t)UINT32_MAX - 100;
    uint64_t time = 123;
    tunuino_capture_event event;
    tunuino_raw_capture pair[2] = {{20,TUNUINO_CAPTURE_CAM},{UINT32_MAX - 20,TUNUINO_CAPTURE_CRANK}};
    assert(tunuino_capture_source_init(&source,&config,(uint32_t)origin,origin,1000,500) == TUNUINO_SOURCE_OK);
    assert(tunuino_capture_source_sample(&source,50,pair,2,0,&time) == TUNUINO_SOURCE_OK);
    assert(time == (uint64_t)UINT32_MAX + 51);
    assert(tunuino_capture_take(&source.processor.queue,&event) == TUNUINO_CAPTURE_OK);
    assert(event.kind == TUNUINO_CAPTURE_CRANK && event.time_us == (uint64_t)UINT32_MAX - 20);
    assert(tunuino_capture_take(&source.processor.queue,&event) == TUNUINO_CAPTURE_OK);
    assert(event.kind == TUNUINO_CAPTURE_CAM && event.time_us == (uint64_t)UINT32_MAX + 21);
    pair[0].counter = pair[1].counter = 60;
    assert(tunuino_capture_source_sample(&source,70,pair,2,0,&time) == TUNUINO_SOURCE_OK);
    assert(tunuino_capture_take(&source.processor.queue,&event) == TUNUINO_CAPTURE_OK && event.kind == TUNUINO_CAPTURE_CRANK);
    assert(tunuino_capture_take(&source.processor.queue,&event) == TUNUINO_CAPTURE_OK && event.kind == TUNUINO_CAPTURE_CAM);
    assert(tunuino_capture_source_sample(&source,80,NULL,0,0,&time) == TUNUINO_SOURCE_OK);
    assert(time == (uint64_t)UINT32_MAX + 81);
    /* A genuinely older cam must precede crank, independent of channel order. */
    pair[0] = (tunuino_raw_capture){90,TUNUINO_CAPTURE_CRANK};
    pair[1] = (tunuino_raw_capture){85,TUNUINO_CAPTURE_CAM};
    assert(tunuino_capture_source_sample(&source,95,pair,2,0,&time) == TUNUINO_SOURCE_OK);
    assert(tunuino_capture_take(&source.processor.queue,&event) == TUNUINO_CAPTURE_OK && event.kind == TUNUINO_CAPTURE_CAM);
    assert(tunuino_capture_take(&source.processor.queue,&event) == TUNUINO_CAPTURE_OK && event.kind == TUNUINO_CAPTURE_CRANK);
    /* The valid first capture must not publish if the second is future. */
    pair[0].counter = 90; pair[1].counter = 101;
    unsigned written = atomic_load(&source.processor.queue.written);
    assert(tunuino_capture_source_sample(&source,100,pair,2,0,&time) == TUNUINO_SOURCE_CAPTURE && time == 0);
    assert(atomic_load(&source.processor.queue.written) == written);
    assert(tunuino_capture_source_sample(&source,110,NULL,0,0,&time) == TUNUINO_SOURCE_CAPTURE && time == 0);
    initialize();
    assert(tunuino_capture_source_sample(&source,(uint32_t)now_us,NULL,0,1,&time) == TUNUINO_SOURCE_LOST && time == 0);
    no_rpm(TUNUINO_CAPTURE_SOURCE_FAULT);
    initialize();
    pair[0] = pair[1] = (tunuino_raw_capture){(uint32_t)event_us,TUNUINO_CAPTURE_CRANK};
    assert(tunuino_capture_source_sample(&source,(uint32_t)now_us,pair,2,0,&time) == TUNUINO_SOURCE_ARGUMENT && time == 0);
    no_rpm(TUNUINO_CAPTURE_SOURCE_FAULT);
    initialize();
    assert(tunuino_capture_source_sample(&source,(uint32_t)now_us,NULL,1,0,&time) == TUNUINO_SOURCE_ARGUMENT && time == 0);
    no_rpm(TUNUINO_CAPTURE_SOURCE_FAULT);
    initialize();
    assert(tunuino_capture_source_sample(&source,(uint32_t)now_us,pair,3,0,&time) == TUNUINO_SOURCE_ARGUMENT && time == 0);
    no_rpm(TUNUINO_CAPTURE_SOURCE_FAULT);
    initialize();
    assert(tunuino_capture_source_sample(&source,(uint32_t)now_us,NULL,0,0,NULL) == TUNUINO_SOURCE_ARGUMENT);
    assert(tunuino_capture_source_sample(&source,(uint32_t)now_us,NULL,0,0,&time) == TUNUINO_SOURCE_OK);
    assert(tunuino_capture_source_sample(NULL,0,NULL,0,0,&time) == TUNUINO_SOURCE_ARGUMENT && time == 0);
    initialize();
    for (unsigned i = 0; i < TUNUINO_CAPTURE_CAPACITY - 1; ++i)
        assert(tunuino_capture_source_push(&source,(uint32_t)event_us,TUNUINO_CAPTURE_CRANK) == TUNUINO_SOURCE_OK);
    pair[0] = (tunuino_raw_capture){(uint32_t)event_us,TUNUINO_CAPTURE_CAM};
    pair[1] = (tunuino_raw_capture){(uint32_t)event_us,TUNUINO_CAPTURE_CRANK};
    assert(tunuino_capture_source_sample(&source,(uint32_t)now_us,pair,2,0,&time) == TUNUINO_SOURCE_QUEUE && time == 0);
    no_rpm(TUNUINO_CAPTURE_OVERFLOW);
    initialize();
    assert(tunuino_capture_source_sample(&source,(uint32_t)(now_us + 10001),NULL,0,0,&time) == TUNUINO_SOURCE_CLOCK && time == 0);
    no_rpm(TUNUINO_CAPTURE_SOURCE_FAULT);
    puts("C capture snapshot: wrap ordering, crank/cam ties, heartbeat, prevalidation and sticky loss passed");
}
int main(void) {
    uint64_t time = 99;
    assert(tunuino_capture_source_observe(&source,0,&time) == TUNUINO_SOURCE_ARGUMENT && time == 0);
    assert(tunuino_capture_source_push(NULL,0,TUNUINO_CAPTURE_CRANK) == TUNUINO_SOURCE_ARGUMENT);
    assert(tunuino_capture_source_lost(NULL) == TUNUINO_SOURCE_ARGUMENT);
    assert(tunuino_capture_abort(NULL) == TUNUINO_CAPTURE_ARGUMENT);
    assert(tunuino_capture_source_init(NULL,&config,0,0,100,100) == TUNUINO_SOURCE_ARGUMENT);
    assert(tunuino_capture_source_init(&source,&config,0,0,0,100) == TUNUINO_SOURCE_ARGUMENT);
    assert(tunuino_capture_source_init(&source,&config,0,0,100,0) == TUNUINO_SOURCE_ARGUMENT);
    assert(tunuino_capture_source_init(&source,&config,0,0,100,UINT32_MAX) == TUNUINO_SOURCE_ARGUMENT);
    assert(tunuino_capture_source_init(&source,NULL,0,0,100,100) == TUNUINO_SOURCE_ARGUMENT);
    initialize();
    assert(tunuino_capture_source_observe(&source,(uint32_t)now_us,NULL) == TUNUINO_SOURCE_ARGUMENT);
    assert(tunuino_capture_source_push(&source,(uint32_t)event_us,TUNUINO_CAPTURE_CAM) == TUNUINO_SOURCE_OK);
    assert(tunuino_capture_source_lost(&source) == TUNUINO_SOURCE_LOST);
    no_rpm(TUNUINO_CAPTURE_SOURCE_FAULT);
    assert(tunuino_capture_source_push(&source,(uint32_t)event_us,TUNUINO_CAPTURE_CRANK) == TUNUINO_SOURCE_LOST);
    assert(tunuino_capture_source_observe(&source,(uint32_t)now_us,&time) == TUNUINO_SOURCE_LOST && time == 0);
    assert(tunuino_capture_source_lost(&source) == TUNUINO_SOURCE_LOST);
    initialize();
    assert(tunuino_capture_source_observe(&source,(uint32_t)(now_us + 10001),&time) == TUNUINO_SOURCE_CLOCK && time == 0);
    no_rpm(TUNUINO_CAPTURE_SOURCE_FAULT);
    assert(tunuino_capture_source_lost(&source) == TUNUINO_SOURCE_CLOCK);
    initialize();
    assert(tunuino_capture_source_push(&source,(uint32_t)(now_us - 501),TUNUINO_CAPTURE_CRANK) == TUNUINO_SOURCE_CAPTURE);
    no_rpm(TUNUINO_CAPTURE_SOURCE_FAULT);
    initialize();
    assert(tunuino_capture_source_push(&source,(uint32_t)(now_us + 1),TUNUINO_CAPTURE_CRANK) == TUNUINO_SOURCE_CAPTURE);
    no_rpm(TUNUINO_CAPTURE_SOURCE_FAULT);
    initialize();
    assert(tunuino_capture_source_push(&source,(uint32_t)(event_us - 1),TUNUINO_CAPTURE_CRANK) == TUNUINO_SOURCE_QUEUE);
    no_rpm(TUNUINO_CAPTURE_ORDER);
    assert(tunuino_capture_abort(&source.processor.queue) == TUNUINO_CAPTURE_ORDER);
    initialize();
    assert(tunuino_capture_source_push(&source,(uint32_t)event_us,(tunuino_capture_kind)99) == TUNUINO_SOURCE_QUEUE);
    no_rpm(TUNUINO_CAPTURE_ARGUMENT);
    initialize();
    for (unsigned i = 0; i < TUNUINO_CAPTURE_CAPACITY; ++i)
        assert(tunuino_capture_source_push(&source,(uint32_t)event_us,TUNUINO_CAPTURE_CRANK) == TUNUINO_SOURCE_OK);
    assert(tunuino_capture_source_push(&source,(uint32_t)event_us,TUNUINO_CAPTURE_CRANK) == TUNUINO_SOURCE_QUEUE);
    no_rpm(TUNUINO_CAPTURE_OVERFLOW);
    assert(tunuino_capture_source_lost(&source) == TUNUINO_SOURCE_QUEUE);
    initialize();
    /* Heartbeats keep the clock valid while lack of edges expires RPM. */
    for (unsigned i = 0; i < 300; ++i) {
        now_us += 1000;
        assert(tunuino_capture_source_observe(&source,(uint32_t)now_us,&time) == TUNUINO_SOURCE_OK && time == now_us);
    }
    tunuino_position_reading reading;
    assert(tunuino_capture_processor_read(&source.processor,now_us,&reading) == TUNUINO_PROCESS_NO_SIGNAL && !reading.crank_valid);
    assert(tunuino_capture_state(&source.processor.queue) == TUNUINO_CAPTURE_OK);
    initialize();
    assert(tunuino_capture_source_init(&source,&config,0,0,100,0) == TUNUINO_SOURCE_ARGUMENT);
    assert(tunuino_capture_processor_read(&source.processor,0,&reading) == TUNUINO_PROCESS_ARGUMENT && !reading.crank_valid);
    batch_checks();
    puts("C capture source: clock/queue bridge, wrap, delayed edges, reported loss, sticky faults, RPM invalidation and quiescent recovery passed");
    return 0;
}
