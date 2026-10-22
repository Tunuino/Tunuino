/* Copyright (c) 2026 Tunuino. */
#include "tunuino/live_mailbox.h"
#include <assert.h>
#include <stdio.h>
#if !defined(_WIN32) && !defined(__STDC_NO_THREADS__)
#include <threads.h>
#define STRESS_COUNT 200000u
#define EPOCH (UINT64_C(1) << 40)
typedef struct { tunuino_live_mailbox *mailbox; atomic_uint observed; } stress_context;
static tunuino_live_sample patterned(unsigned i) {
    return (tunuino_live_sample){EPOCH + i,i,i ^ 0x12345678u,i * 3u,
        -(int32_t)i,i % 7u ? TUNUINO_LIVE_ALL_VALID : 0};
}
static int producer(void *argument) {
    stress_context *context = argument;
    for (unsigned i = 1; i <= STRESS_COUNT; ++i) {
        tunuino_live_sample sample = patterned(i);
        assert(tunuino_live_mailbox_publish(context->mailbox,&sample) == TUNUINO_MAILBOX_READY);
        /* Ensure many ownership handoffs, even if the producer is much faster. */
        if (i % 1000u == 0 && i % 7u != 0)
            while (atomic_load_explicit(&context->observed,memory_order_acquire) < i) thrd_yield();
    }
    return 0;
}
static int consumer(void *argument) {
    stress_context *context = argument;
    unsigned last = 0;
    while (last < STRESS_COUNT) {
        tunuino_live_sample sample;
        tunuino_live_mailbox_status status = tunuino_live_mailbox_read(context->mailbox,
            EPOCH + STRESS_COUNT,STRESS_COUNT + 1,&sample);
        if (status == TUNUINO_MAILBOX_NOT_READY) {
            assert(sample.time_ms == 0 && sample.valid_mask == 0 && sample.rpm_milli == 0);
            thrd_yield();
            continue;
        }
        assert(status == TUNUINO_MAILBOX_READY);
        assert(sample.time_ms >= EPOCH && sample.time_ms <= EPOCH + STRESS_COUNT);
        unsigned i = (unsigned)(sample.time_ms - EPOCH);
        assert(i >= last && i % 7u != 0);
        assert(sample.rpm_milli == i && sample.map_milli_kpa == (i ^ 0x12345678u)
            && sample.lambda_million == i * 3u && sample.coolant_milli_c == -(int32_t)i);
        last = i;
        atomic_store_explicit(&context->observed,last,memory_order_release);
    }
    return 0;
}
#endif
int main(void) {
    tunuino_live_mailbox mailbox = {0};
    tunuino_live_sample sample = {100,1500000,100000,1000000,-10000,TUNUINO_LIVE_ALL_VALID};
    tunuino_live_sample out = sample;
    assert(tunuino_live_mailbox_init(NULL) == TUNUINO_MAILBOX_ARGUMENT);
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_ARGUMENT);
    assert(tunuino_live_mailbox_read(&mailbox,100,10,&out) == TUNUINO_MAILBOX_ARGUMENT && out.valid_mask == 0);
    assert(tunuino_live_mailbox_init(&mailbox) == TUNUINO_MAILBOX_READY);
    assert(tunuino_live_mailbox_read(&mailbox,100,10,&out) == TUNUINO_MAILBOX_NOT_READY && out.time_ms == 0);
    assert(tunuino_live_mailbox_publish(NULL,&sample) == TUNUINO_MAILBOX_ARGUMENT);
    assert(tunuino_live_mailbox_publish(&mailbox,NULL) == TUNUINO_MAILBOX_ARGUMENT);
    assert(tunuino_live_mailbox_read(NULL,100,10,&out) == TUNUINO_MAILBOX_ARGUMENT && out.rpm_milli == 0);
    assert(tunuino_live_mailbox_read(&mailbox,100,10,NULL) == TUNUINO_MAILBOX_ARGUMENT);
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    sample.rpm_milli = 999;
    assert(tunuino_live_mailbox_read(&mailbox,100,10,&out) == TUNUINO_MAILBOX_READY && out.rpm_milli == 1500000);
    assert(tunuino_live_mailbox_read(&mailbox,109,10,&out) == TUNUINO_MAILBOX_READY && out.coolant_milli_c == -10000);
    assert(tunuino_live_mailbox_read(&mailbox,110,10,&out) == TUNUINO_MAILBOX_NOT_READY && out.valid_mask == 0);
    assert(tunuino_live_mailbox_read(&mailbox,110,0,&out) == TUNUINO_MAILBOX_ARGUMENT && out.time_ms == 0);
    /* Replacing unread snapshots is intentional; the last one must win. */
    for (unsigned i = 0; i < 1000; ++i) {
        sample.time_ms = 200 + i;
        assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    }
    assert(tunuino_live_mailbox_read(&mailbox,1199,10,&out) == TUNUINO_MAILBOX_READY && out.time_ms == 1199);
    sample.time_ms = 1200; sample.valid_mask = 0;
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    assert(tunuino_live_mailbox_read(&mailbox,1200,10,&out) == TUNUINO_MAILBOX_NOT_READY && out.rpm_milli == 0);
    sample.valid_mask = TUNUINO_LIVE_ALL_VALID;
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    assert(tunuino_live_mailbox_read(&mailbox,1200,10,&out) == TUNUINO_MAILBOX_READY);
    sample.valid_mask = TUNUINO_LIVE_RPM_VALID;
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    assert(tunuino_live_mailbox_read(&mailbox,1200,10,&out) == TUNUINO_MAILBOX_NOT_READY && out.lambda_million == 0);
    sample.valid_mask = TUNUINO_LIVE_ALL_VALID | 16u;
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    assert(tunuino_live_mailbox_read(&mailbox,1200,10,&out) == TUNUINO_MAILBOX_NOT_READY && out.valid_mask == 0);
    sample.valid_mask = TUNUINO_LIVE_ALL_VALID;
    sample.time_ms = UINT64_MAX;
    assert(tunuino_live_mailbox_publish(&mailbox,&sample) == TUNUINO_MAILBOX_READY);
    assert(tunuino_live_mailbox_read(&mailbox,UINT64_MAX - 1,10,&out) == TUNUINO_MAILBOX_NOT_READY && out.time_ms == 0);
    assert(tunuino_live_mailbox_read(&mailbox,UINT64_MAX,10,&out) == TUNUINO_MAILBOX_READY && out.time_ms == UINT64_MAX);
    assert(tunuino_live_mailbox_init(&mailbox) == TUNUINO_MAILBOX_READY);
    assert(tunuino_live_mailbox_read(&mailbox,UINT64_MAX,10,&out) == TUNUINO_MAILBOX_NOT_READY);
#if !defined(_WIN32) && !defined(__STDC_NO_THREADS__)
    thrd_t writer,reader; int result;
    stress_context context = {.mailbox = &mailbox};
    atomic_init(&context.observed,0u);
    assert(thrd_create(&writer,producer,&context) == thrd_success);
    assert(thrd_create(&reader,consumer,&context) == thrd_success);
    assert(thrd_join(writer,&result) == thrd_success && result == 0);
    assert(thrd_join(reader,&result) == thrd_success && result == 0);
    puts("C live mailbox: 200000 concurrent publications, coherent fields and monotonic reads passed");
#endif
    puts("C live mailbox: copied snapshots, latest replacement, freshness, invalidation and recovery passed");
    return 0;
}
