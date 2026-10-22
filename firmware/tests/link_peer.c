/* Copyright (c) 2026 Tunuino. Test-only process peer; no physical ECU defaults. */
#include "tunuino/link.h"
#include "tunuino/telemetry.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
static tunuino_link link;
static tunuino_map map;
static tunuino_map_snapshot ve, ignition;
static tunuino_capture_processor captures;
static uint64_t capture_time;
static const tunuino_position_config capture_config = {{36,1,30,15000,300000},9,11,0};
static int synthetic_edge(uint64_t interval) {
    capture_time += interval;
    if (tunuino_capture_processor_push(&captures,(tunuino_capture_event){capture_time,TUNUINO_CAPTURE_CRANK}) != TUNUINO_CAPTURE_OK) { return 0; }
    size_t count;
    return tunuino_capture_processor_drain(&captures,capture_time,64,&count) == TUNUINO_PROCESS_READY && count == 1;
}
static int synthetic_telemetry(tunuino_live_sample *live) {
    tunuino_telemetry_sensors sensors = {capture_time,capture_time,capture_time,98765,1023456,-12345,TUNUINO_TELEMETRY_SENSOR_MASK};
    tunuino_telemetry_policy policy = {10000,10000,10000};
    return tunuino_telemetry_build(&captures,&sensors,&policy,capture_time,live) != TUNUINO_TELEMETRY_ARGUMENT;
}
static int start_captures(tunuino_live_sample *live) {
    capture_time = 0;
    if (tunuino_capture_processor_init(&captures,&capture_config,capture_time) != TUNUINO_PROCESS_NO_SIGNAL || !synthetic_edge(0)) { return 0; }
    for (unsigned i = 0; i < 5; ++i) { if (!synthetic_edge(1000)) { return 0; } }
    if (!synthetic_edge(2000)) { return 0; }
    for (unsigned i = 1; i < 35; ++i) { if (!synthetic_edge(1000)) { return 0; } }
    return synthetic_edge(2000) && synthetic_telemetry(live) && live->valid_mask == TUNUINO_LIVE_ALL_VALID;
}
int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[1], "proteus") != 0 && strcmp(argv[1], "uaefi") != 0)) { return 2; }
    const char *mode = argv[2];
    if (strcmp(mode,"normal") != 0 && strcmp(mode,"stale-map") != 0
        && strcmp(mode,"bad-crc") != 0 && strcmp(mode,"truncated") != 0
        && strcmp(mode,"drop-mid-map") != 0 && strcmp(mode,"not-ready") != 0
        && strcmp(mode,"crank-live") != 0 && strcmp(mode,"crank-stall") != 0) { return 2; }
#ifdef _WIN32
    if (_setmode(_fileno(stdin),_O_BINARY) < 0 || _setmode(_fileno(stdout),_O_BINARY) < 0) { return 3; }
#endif
    tunuino_identity identity = {TUNUINO_PROTEUS,TUNUINO_STM32_F4,1,0,1,0,3,{1,2,3,4,5,6,7,8,9,10,11,12}};
    if (strcmp(argv[1],"uaefi") == 0) { identity.family = TUNUINO_UAEFI; identity.mcu_series = TUNUINO_STM32_F7; }
    tunuino_live_sample live = {1234,950125,98765,1023456,-12345,15};
    int use_captures = strcmp(mode,"crank-live") == 0 || strcmp(mode,"crank-stall") == 0;
    int stalled = 0;
    if (use_captures && !start_captures(&live)) { return 11; }
    map.rpm_count = map.load_count = 32;
    for (unsigned i = 0; i < 32; ++i) { map.rpm[i] = (int32_t)(i * 250); map.load_milli_kpa[i] = (int32_t)(i * 10000); }
    for (unsigned kind = 1; kind <= 2; ++kind) {
        map.kind = (tunuino_map_kind)kind;
        for (unsigned r = 0; r < 32; ++r) {
            for (unsigned c = 0; c < 32; ++c) { map.cells[r][c] = (int32_t)(r * 1000 + c) + (kind == 1 ? 40000 : -15000); }
        }
        if (tunuino_map_snapshot_prepare(kind == 1 ? &ve : &ignition,&map,7) != TUNUINO_MAP_OK) { return 4; }
    }
    if (strcmp(mode,"not-ready") == 0) { live.valid_mask = 0; ve.generation = ignition.generation = 0; }
    tunuino_dispatch_context context = {&identity,&live,&ve,&ignition};
    /* Deterministic synthetic time; receiver/transmitter deadline tests are separate. */
    if (tunuino_link_init(&link,&context,1000,1000,0) != TUNUINO_LINK_RECEIVING) { return 5; }
    int byte;
    while ((byte = getchar()) != EOF) {
        uint8_t input = (uint8_t)byte; size_t consumed;
        tunuino_link_status status = tunuino_link_feed(&link,&input,1,0,&consumed);
        if (consumed != 1 || (status != TUNUINO_LINK_RECEIVING && status != TUNUINO_LINK_RESPONDING)) { return 6; }
        if (status != TUNUINO_LINK_RESPONDING) { continue; }
        const uint8_t *view; size_t remaining;
        if (tunuino_link_tx_view(&link,0,&view,&remaining) != TUNUINO_LINK_RESPONDING
            || remaining > TUNUINO_DISPATCH_RESPONSE_MAX) { return 7; }
        uint8_t output[TUNUINO_DISPATCH_RESPONSE_MAX]; size_t total = remaining;
        memcpy(output,view,total);
        int meta = total == 20 && output[2] == 0 && output[3] == 1 && output[4] == 0;
        int block = total >= 19 && output[2] == 0 && output[3] == 1 && output[4] == 1;
        if (strcmp(mode,"bad-crc") == 0) { output[total - 1] ^= 1; }
        int truncate = strcmp(mode,"truncated") == 0 || (strcmp(mode,"drop-mid-map") == 0 && block);
        size_t limit = truncate ? total / 2 : total;
        size_t offset = 0;
        while (offset < limit) {
            size_t count = limit - offset; if (count > 3) { count = 3; }
            if (fwrite(output + offset,1,count,stdout) != count || fflush(stdout) != 0) { return 8; }
            offset += count;
            if (tunuino_link_tx_advance(&link,count,0) != (offset == total ? TUNUINO_LINK_RECEIVING : TUNUINO_LINK_RESPONDING)) { return 9; }
        }
        if (truncate) { tunuino_link_abort(&link); return 0; }
        if (meta && strcmp(mode,"stale-map") == 0) {
            /* Real handler emits SNAPSHOT_CHANGED on the next chunk request. */
            if (output[5] == 1) { ++ve.generation; } else { ++ignition.generation; }
        }
        if (use_captures) {
            if (strcmp(mode,"crank-stall") == 0 && total == 34 && output[2] == 0 && output[3] == 1 && output[4] == 15) { stalled = 1; }
            if (stalled) {
                capture_time += capture_config.crank.timeout_us;
            } else {
                tunuino_position_reading position;
                if (tunuino_capture_processor_read(&captures,capture_time,&position) != TUNUINO_PROCESS_READY || !position.crank_valid) { return 12; }
                if (!synthetic_edge(position.tooth_index == 34 ? 2000 : 1000)) { return 13; }
            }
            if (!synthetic_telemetry(&live)) { return 14; }
        } else {
            ++live.time_ms;
        }
    }
    tunuino_link_abort(&link);
    return ferror(stdin) ? 10 : 0;
}
