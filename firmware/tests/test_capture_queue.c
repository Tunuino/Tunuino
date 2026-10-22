/* Copyright (c) 2026 Tunuino. */
#include "tunuino/capture_queue.h"
#include "tunuino/position.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#if !defined(_WIN32) && !defined(__STDC_NO_THREADS__)
#include <threads.h>
#define STRESS_COUNT 200000u
static int producer(void *argument) {
    tunuino_capture_queue *q = argument;
    for (unsigned i = 0; i < STRESS_COUNT; ++i) {
        while (tunuino_capture_count(q) == TUNUINO_CAPTURE_CAPACITY) { thrd_yield(); }
        assert(tunuino_capture_push(q,(tunuino_capture_event){(uint64_t)i * 2,i % 2 ? TUNUINO_CAPTURE_CAM : TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    }
    return 0;
}
static int consumer(void *argument) {
    tunuino_capture_queue *q = argument;
    for (unsigned i = 0; i < STRESS_COUNT; ++i) {
        tunuino_capture_event event;
        tunuino_capture_status status;
        do { status = tunuino_capture_take(q,&event); if (status == TUNUINO_CAPTURE_EMPTY) { thrd_yield(); } } while (status == TUNUINO_CAPTURE_EMPTY);
        assert(status == TUNUINO_CAPTURE_OK && event.time_us == (uint64_t)i * 2);
        assert(event.kind == (i % 2 ? TUNUINO_CAPTURE_CAM : TUNUINO_CAPTURE_CRANK));
    }
    return 0;
}
#endif
static tunuino_capture_queue queue;
static tunuino_position position;
static uint64_t now;
static tunuino_position_status deliver(tunuino_capture_kind kind, uint64_t interval) {
    now += interval;
    assert(tunuino_capture_push(&queue,(tunuino_capture_event){now,kind}) == TUNUINO_CAPTURE_OK);
    tunuino_capture_event event;
    assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_count(&queue) == 0);
    return event.kind == TUNUINO_CAPTURE_CRANK ? tunuino_position_crank_edge(&position,event.time_us)
        : tunuino_position_cam_edge(&position,event.time_us);
}
int main(void) {
    tunuino_capture_event event = {99,TUNUINO_CAPTURE_CAM};
    assert(tunuino_capture_init(&queue) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_EMPTY && event.time_us == 0 && event.kind == 0);
    /* Counter wrap with all 64 slots in use; no reserved empty slot. */
    atomic_store(&queue.written,UINT_MAX - 31u); atomic_store(&queue.taken,UINT_MAX - 31u);
    for (unsigned i = 0; i < TUNUINO_CAPTURE_CAPACITY; ++i) {
        assert(tunuino_capture_push(&queue,(tunuino_capture_event){i,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
        assert(tunuino_capture_count(&queue) == i + 1u);
    }
    for (unsigned i = 0; i < TUNUINO_CAPTURE_CAPACITY; ++i) {
        assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_OK && event.time_us == i && event.kind == TUNUINO_CAPTURE_CRANK);
    }
    assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_EMPTY && event.kind == 0);
    for (unsigned round = 0; round < 100; ++round) {
        for (unsigned i = 0; i < TUNUINO_CAPTURE_CAPACITY; ++i) {
            assert(tunuino_capture_push(&queue,(tunuino_capture_event){1000u + round * 64u + i,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
        }
        for (unsigned i = 0; i < TUNUINO_CAPTURE_CAPACITY; ++i) {
            assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_OK && event.time_us == 1000u + round * 64u + i);
        }
    }
    assert(tunuino_capture_init(&queue) == TUNUINO_CAPTURE_OK);
    for (unsigned i = 0; i < TUNUINO_CAPTURE_CAPACITY; ++i) {
        assert(tunuino_capture_push(&queue,(tunuino_capture_event){i,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    }
    assert(tunuino_capture_push(&queue,(tunuino_capture_event){100,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OVERFLOW);
    assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_OVERFLOW && event.kind == 0 && event.time_us == 0);
    assert(tunuino_capture_push(&queue,(tunuino_capture_event){101,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OVERFLOW);
    assert(tunuino_capture_state(&queue) == TUNUINO_CAPTURE_OVERFLOW && tunuino_capture_count(&queue) == 0);
    assert(tunuino_capture_init(&queue) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_push(&queue,(tunuino_capture_event){100,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_push(&queue,(tunuino_capture_event){99,TUNUINO_CAPTURE_CAM}) == TUNUINO_CAPTURE_ORDER);
    assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_ORDER && event.kind == 0);
    assert(tunuino_capture_init(&queue) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_push(&queue,(tunuino_capture_event){UINT64_MAX,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_push(&queue,(tunuino_capture_event){UINT64_MAX,TUNUINO_CAPTURE_CAM}) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_OK && event.kind == TUNUINO_CAPTURE_CRANK);
    assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_OK && event.kind == TUNUINO_CAPTURE_CAM);
    assert(tunuino_capture_push(&queue,(tunuino_capture_event){UINT64_MAX,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_ORDER);
    assert(tunuino_capture_init(&queue) == TUNUINO_CAPTURE_OK);
    assert(tunuino_capture_push(&queue,(tunuino_capture_event){0,0}) == TUNUINO_CAPTURE_ARGUMENT);
    assert(tunuino_capture_take(&queue,&event) == TUNUINO_CAPTURE_ARGUMENT && event.kind == 0);
    assert(tunuino_capture_init(NULL) == TUNUINO_CAPTURE_ARGUMENT);
    assert(tunuino_capture_state(NULL) == TUNUINO_CAPTURE_ARGUMENT);
    assert(tunuino_capture_count(NULL) == 0);
    assert(tunuino_capture_push(NULL,(tunuino_capture_event){1,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_ARGUMENT);
    assert(tunuino_capture_take(NULL,&event) == TUNUINO_CAPTURE_ARGUMENT && event.kind == 0);
    assert(tunuino_capture_take(&queue,NULL) == TUNUINO_CAPTURE_ARGUMENT);
    /* Synthetic queue -> real crank/cam decoder; no hardware or output driver. */
    assert(tunuino_capture_init(&queue) == TUNUINO_CAPTURE_OK);
    tunuino_position_config config = {{36,1,30,15000,300000},9,11,0}; now = 0;
    assert(tunuino_position_init(&position,&config,now) == TUNUINO_POSITION_ACQUIRING);
    deliver(TUNUINO_CAPTURE_CRANK,0);
    for (unsigned i = 0; i < 5; ++i) { deliver(TUNUINO_CAPTURE_CRANK,1000); }
    deliver(TUNUINO_CAPTURE_CRANK,2000);
    for (unsigned i = 1; i < 35; ++i) { deliver(TUNUINO_CAPTURE_CRANK,1000); }
    assert(deliver(TUNUINO_CAPTURE_CRANK,2000) == TUNUINO_POSITION_CRANK_ONLY);
    for (unsigned i = 0; i < 10; ++i) { deliver(TUNUINO_CAPTURE_CRANK,1000); }
    assert(deliver(TUNUINO_CAPTURE_CAM,0) == TUNUINO_POSITION_CRANK_ONLY);
    for (unsigned i = 0; i < 70; ++i) {
        tunuino_position_reading r;
        tunuino_position_read(&position,now,&r);
        deliver(TUNUINO_CAPTURE_CRANK,r.tooth_index == 34 ? 2000 : 1000);
    }
    assert(deliver(TUNUINO_CAPTURE_CAM,0) == TUNUINO_POSITION_SYNCED);
    tunuino_position_reading reading;
    assert(tunuino_position_read(&position,now,&reading) == TUNUINO_POSITION_SYNCED && reading.cycle_valid);
#if !defined(_WIN32) && !defined(__STDC_NO_THREADS__)
    assert(tunuino_capture_init(&queue) == TUNUINO_CAPTURE_OK);
    thrd_t writer,reader; int result;
    assert(thrd_create(&writer,producer,&queue) == thrd_success);
    assert(thrd_create(&reader,consumer,&queue) == thrd_success);
    assert(thrd_join(writer,&result) == thrd_success && result == 0);
    assert(thrd_join(reader,&result) == thrd_success && result == 0);
    assert(tunuino_capture_count(&queue) == 0 && tunuino_capture_state(&queue) == TUNUINO_CAPTURE_OK);
    puts("C capture queue: 200000 events across concurrent producer/consumer passed");
#endif
    puts("C capture queue: full capacity, unsigned wrap, overflow latch, timestamp order and crank/cam integration passed");
    return 0;
}
