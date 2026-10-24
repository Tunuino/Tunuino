/* Copyright (c) 2026 Tunuino. Values and maps here are synthetic, never ECU defaults. */
#include "model.h"
#include "tunuino/bench_tunerstudio.h"
#include "tunuino/telemetry.h"
#include <stdio.h>
#include <string.h>

static int edge(sim_model *m, uint64_t time) {
    tunuino_raw_capture capture = {(uint32_t)time,TUNUINO_CAPTURE_CRANK};
    uint64_t observed;
    size_t count;
    if (tunuino_capture_source_sample(&m->source,(uint32_t)time,&capture,1,0,&observed)
        != TUNUINO_SOURCE_OK || observed != time) return 0;
    if (tunuino_capture_processor_drain(&m->source.processor,time,64,&count) != TUNUINO_PROCESS_READY || count != 1) return 0;
    m->last_edge_us = m->now_us = time;
    return 1;
}

static int telemetry(sim_model *m) {
    tunuino_telemetry_sensors sensors = {m->now_us,m->now_us,m->now_us,
        60000,1000000,85000,TUNUINO_TELEMETRY_SENSOR_MASK};
    tunuino_telemetry_policy policy = {10000,10000,10000};
    if (m->scenario == SIM_SENSOR_LOSS && m->samples >= 5 && m->samples < 10) {
        sensors.valid_mask &= (uint8_t)~TUNUINO_LIVE_MAP_VALID;
    }
    return tunuino_telemetry_build(&m->source.processor,&sensors,&policy,m->now_us,&m->live)
        != TUNUINO_TELEMETRY_ARGUMENT;
}

static int example_maps(sim_model *m) {
    tunuino_map map = {0};
    const int32_t rpm[] = {0,1000,3000,6000}, load[] = {20000,60000,100000,200000};
    map.rpm_count = map.load_count = 4;
    memcpy(map.rpm,rpm,sizeof(rpm)); memcpy(map.load_milli_kpa,load,sizeof(load));
    for (unsigned kind = 1; kind <= 2; ++kind) {
        map.kind = (tunuino_map_kind)kind;
        for (unsigned r = 0; r < 4; ++r) for (unsigned c = 0; c < 4; ++c) {
            map.cells[r][c] = kind == 1 ? (int32_t)(50000 + r * 10000 + c * 5000)
                                      : (int32_t)(5000 + c * 5000) - (int32_t)r * 1000;
        }
        if (tunuino_map_snapshot_prepare(kind == 1 ? &m->ve : &m->ignition,&map,1) != TUNUINO_MAP_OK) return 0;
    }
    return 1;
}

int sim_model_init(sim_model *m, tunuino_family family, unsigned rpm, sim_scenario scenario, int bench) {
    if (m == NULL || rpm < 600 || rpm > 10000 || (family != TUNUINO_PROTEUS && family != TUNUINO_UAEFI)
        || (bench && family != TUNUINO_PROTEUS)) return 0;
    memset(m,0,sizeof(*m));
    /* ASCII 'SIMULATOR001': recognizable, deliberately synthetic UID. */
    tunuino_identity id = {family,TUNUINO_STM32_F4,7,0,1,0,3,
        {0x53,0x49,0x4d,0x55,0x4c,0x41,0x54,0x4f,0x52,0x30,0x30,0x31}};
    m->identity = id;
    if (bench) {
        m->identity.firmware_major = TUNUINO_BENCH_MAJOR;
        m->identity.firmware_minor = TUNUINO_BENCH_MINOR;
        m->identity.firmware_patch = TUNUINO_BENCH_PATCH;
        m->identity.capabilities = 0;
    }
    m->scenario = scenario; m->interval_us = (60000000u + rpm * 18u) / (rpm * 36u);
    m->now_us = scenario == SIM_WRAP ? (uint64_t)UINT32_MAX - 50000u : 0;
    const tunuino_position_config config = {{36,1,30,15000,300000},9,11,0};
    if (tunuino_capture_source_init(&m->source,&config,(uint32_t)m->now_us,m->now_us,10000,500) != TUNUINO_SOURCE_OK
        || !edge(m,m->now_us)) return 0;
    for (unsigned i = 0; i < 80; ++i) {
        uint32_t interval = m->interval_us * (m->tooth == 34 ? 2u : 1u);
        m->tooth = (m->tooth + 1) % 35;
        if (!edge(m,m->last_edge_us + interval)) return 0;
    }
    return example_maps(m) && telemetry(m) && m->live.valid_mask == TUNUINO_LIVE_ALL_VALID;
}

int sim_model_step(sim_model *m) {
    ++m->samples;
    uint64_t target = m->now_us + 100000u;
    int stalled = m->scenario == SIM_STALL && m->samples >= 5 && m->samples < 11;
    if (!stalled && m->was_stalled) {
        m->tooth = 0;
        if (!edge(m,m->now_us)) return 0;
    }
    if (!stalled) {
        while (1) {
            uint64_t next = m->last_edge_us + m->interval_us * (m->tooth == 34 ? 2u : 1u);
            if (next > target) break;
            m->tooth = (m->tooth + 1) % 35;
            if (!edge(m,next)) return 0;
        }
    }
    while (m->now_us < target) {
        uint64_t time = target - m->now_us > 1000 ? m->now_us + 1000 : target;
        uint64_t observed;
        if (tunuino_capture_source_observe(&m->source,(uint32_t)time,&observed) != TUNUINO_SOURCE_OK || observed != time) return 0;
        m->now_us = time;
    }
    m->was_stalled = (unsigned)stalled;
    return telemetry(m);
}

int sim_model_load_map(sim_model *m, const char *path, tunuino_map_kind kind) {
    uint8_t bytes[TUNUINO_MAP_FILE_MAX];
    tunuino_map map;
    FILE *file = fopen(path,"rb");
    if (file == NULL) return 0;
    size_t length = fread(bytes,1,sizeof(bytes),file);
    int valid = !ferror(file) && fgetc(file) == EOF;
    if (fclose(file) != 0) valid = 0;
    if (!valid || tunuino_map_decode(bytes,length,&map) != TUNUINO_MAP_OK || map.kind != kind) return 0;
    return tunuino_map_snapshot_prepare(kind == TUNUINO_MAP_VE ? &m->ve : &m->ignition,&map,2) == TUNUINO_MAP_OK;
}
