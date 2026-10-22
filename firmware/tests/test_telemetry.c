/* Copyright (c) 2026 Tunuino. */
#include "tunuino/telemetry.h"
#include <assert.h>
#include <stdio.h>
static tunuino_capture_processor processor;
static tunuino_position_config config = {{36,1,30,15000,300000},9,11,0};
static tunuino_telemetry_sensors sensors;
static tunuino_telemetry_policy policy = {10000,20000,30000};
static tunuino_live_sample sample;
static uint64_t now;
static void fresh_sensors(void) {
    sensors = (tunuino_telemetry_sensors){now,now,now,98765,1023456,-12345,TUNUINO_TELEMETRY_SENSOR_MASK};
}
static void edge(uint64_t interval) {
    now += interval;
    assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    size_t count;
    assert(tunuino_capture_processor_drain(&processor,now,64,&count) == TUNUINO_PROCESS_READY && count == 1);
}
static void initialize(void) {
    now = 0;
    assert(tunuino_capture_processor_init(&processor,&config,now) == TUNUINO_PROCESS_NO_SIGNAL);
    edge(0);
    for (unsigned i = 0; i < 5; ++i) { edge(1000); }
    edge(2000);
    for (unsigned i = 1; i < 35; ++i) { edge(1000); }
    edge(2000); fresh_sensors();
}
static void check_not_ready_wire(void) {
    uint8_t query[32],reply[64]; size_t qlen,written,length; const uint8_t *body;
    assert(tunuino_frame_encode((const uint8_t *)"TUNUINO:L",9,query,sizeof(query),&qlen) == TUNUINO_FRAME_OK);
    assert(tunuino_live_reply(&sample,query,qlen,reply,sizeof(reply),&written) == TUNUINO_LIVE_OK);
    assert(tunuino_frame_decode(reply,written,&body,&length) == TUNUINO_FRAME_OK && length == 1 && body[0] == 1);
}
int main(void) {
    initialize();
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_READY);
    assert(sample.valid_mask == 15 && sample.rpm_milli == 1666667 && sample.time_ms == now / 1000);
    assert(sample.map_milli_kpa == 98765 && sample.lambda_million == 1023456 && sample.coolant_milli_c == -12345);
    /* No cam phase is claimed or required for a crank RPM log. */
    tunuino_position_reading reading;
    assert(tunuino_capture_processor_read(&processor,now,&reading) == TUNUINO_PROCESS_READY && reading.crank_valid && !reading.cycle_valid);
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now + 9999,&sample) == TUNUINO_TELEMETRY_READY);
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now + 10000,&sample) == TUNUINO_TELEMETRY_NOT_READY);
    assert(sample.valid_mask == 13 && sample.map_milli_kpa == 0); check_not_ready_wire();
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now + 20000,&sample) == TUNUINO_TELEMETRY_NOT_READY);
    assert(sample.valid_mask == 9 && sample.lambda_million == 0);
    now += 30000;
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_NOT_READY);
    assert(sample.valid_mask == 1 && sample.coolant_milli_c == 0);
    fresh_sensors(); sensors.map_time_us = now + 1;
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_NOT_READY && sample.valid_mask == 13);
    fresh_sensors(); sensors.lambda_time_us = now + 1;
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_NOT_READY && sample.valid_mask == 11);
    fresh_sensors(); sensors.coolant_time_us = now + 1;
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_NOT_READY && sample.valid_mask == 7);
    fresh_sensors(); sensors.lambda_million = 0;
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_NOT_READY && sample.valid_mask == 11);
    for (unsigned bit = 2; bit <= 8; bit *= 2) {
        fresh_sensors(); sensors.valid_mask &= (uint8_t)~bit;
        assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_NOT_READY && sample.valid_mask == (15u & ~bit));
    }
    initialize(); now += 1000; fresh_sensors();
    assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,TUNUINO_CAPTURE_CRANK}) == TUNUINO_CAPTURE_OK);
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_NOT_READY && sample.valid_mask == 14 && sample.rpm_milli == 0);
    check_not_ready_wire();
    size_t count;
    assert(tunuino_capture_processor_drain(&processor,now,64,&count) == TUNUINO_PROCESS_READY);
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_READY);
    now += config.crank.timeout_us; fresh_sensors();
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_NOT_READY && sample.valid_mask == 14 && sample.rpm_milli == 0);
    check_not_ready_wire();
    initialize();
    for (unsigned i = 0; i < 65; ++i) {
        now += 1000;
        assert(tunuino_capture_processor_push(&processor,(tunuino_capture_event){now,TUNUINO_CAPTURE_CRANK}) == (i == 64 ? TUNUINO_CAPTURE_OVERFLOW : TUNUINO_CAPTURE_OK));
    }
    fresh_sensors();
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_NOT_READY && sample.valid_mask == 14);
    check_not_ready_wire();
    initialize();
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now - 1,&sample) == TUNUINO_TELEMETRY_NOT_READY && !(sample.valid_mask & TUNUINO_LIVE_RPM_VALID));
    initialize(); sensors.valid_mask |= TUNUINO_LIVE_RPM_VALID;
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_ARGUMENT && sample.valid_mask == 0);
    fresh_sensors();
    assert(tunuino_telemetry_build(NULL,&sensors,&policy,now,&sample) == TUNUINO_TELEMETRY_ARGUMENT && sample.valid_mask == 0);
    assert(tunuino_telemetry_build(&processor,NULL,&policy,now,&sample) == TUNUINO_TELEMETRY_ARGUMENT);
    assert(tunuino_telemetry_build(&processor,&sensors,NULL,now,&sample) == TUNUINO_TELEMETRY_ARGUMENT);
    assert(tunuino_telemetry_build(&processor,&sensors,&policy,now,NULL) == TUNUINO_TELEMETRY_ARGUMENT);
    tunuino_telemetry_policy bad = policy; bad.map_max_age_us = 0;
    assert(tunuino_telemetry_build(&processor,&sensors,&bad,now,&sample) == TUNUINO_TELEMETRY_ARGUMENT && sample.valid_mask == 0);
    bad = policy; bad.coolant_max_age_us = 60000001;
    assert(tunuino_telemetry_build(&processor,&sensors,&bad,now,&sample) == TUNUINO_TELEMETRY_ARGUMENT);
    puts("C telemetry: decoded RPM, sensor freshness, validity masks, capture backlog/loss, rounding and NOT_READY wire checks passed");
    return 0;
}
