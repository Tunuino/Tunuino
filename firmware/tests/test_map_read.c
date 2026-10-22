/* Copyright (c) 2026 Tunuino. */
#include "tunuino/map_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tunuino_map map;
static tunuino_map_snapshot snapshot;
static uint8_t q[32], packet[64], reply[300];
static size_t written, request_length;
static void put(uint8_t *p, uint32_t v, unsigned n) {
    for (unsigned i = 0; i < n; ++i) { p[i] = (uint8_t)(v >> (8u * (n - i - 1u))); }
}
static void request(unsigned op, uint32_t generation, unsigned offset, unsigned count) {
    memcpy(q, "TUNUINO:M", 9);
    q[9] = (uint8_t)op; q[10] = (uint8_t)map.kind;
    put(q + 11, generation, 4); put(q + 15, offset, 2); put(q + 17, count, 2);
    assert(tunuino_frame_encode(q, op == 0 ? 11 : 19, packet, sizeof(packet), &request_length) == TUNUINO_FRAME_OK);
}
static const uint8_t *exchange(size_t expected) {
    assert(tunuino_map_read_reply(&snapshot, packet, request_length, reply, sizeof(reply), &written) == TUNUINO_MAP_READ_OK);
    const uint8_t *body; size_t length;
    assert(tunuino_frame_decode(reply, written, &body, &length) == TUNUINO_FRAME_OK && length == expected);
    return body;
}
static void print_bytes(const uint8_t *bytes, size_t n) {
    for (size_t i = 0; i < n; ++i) { printf("%02x", bytes[i]); }
}
int main(void) {
    map.rpm_count = map.load_count = 32;
    for (unsigned i = 0; i < 32; ++i) {
        map.rpm[i] = (int32_t)(i * 250);
        map.load_milli_kpa[i] = (int32_t)(i * 10000);
    }
    for (unsigned kind = 1; kind <= 2; ++kind) {
        map.kind = (tunuino_map_kind)kind;
        for (unsigned r = 0; r < 32; ++r) {
            for (unsigned c = 0; c < 32; ++c) {
                map.cells[r][c] = kind == 1 ? (int32_t)(r * 1000 + c) : (int32_t)(r * 1000 + c) - 15000;
            }
        }
        assert(tunuino_map_snapshot_prepare(&snapshot, &map, 0xFEDCBA98u) == TUNUINO_MAP_OK);
        assert(snapshot.length == 4368);
        request(0, 0, 0, 0);
        const uint8_t *body = exchange(14);
        assert(body[0] == 0 && body[1] == 1 && body[2] == 0 && body[3] == kind);
        print_bytes(reply, written);
        for (unsigned offset = 0; offset < snapshot.length;) {
            unsigned count = (unsigned)snapshot.length - offset;
            if (count > 256) { count = 256; }
            request(1, snapshot.generation, offset, count);
            body = exchange(12 + count);
            assert(body[2] == 1 && memcmp(body + 12, snapshot.bytes + offset, count) == 0);
            print_bytes(reply, written);
            offset += count;
        }
        putchar('\n'); print_bytes(snapshot.bytes, snapshot.length); putchar('\n');
    }
    request(1, 1, 0, 1); assert(exchange(1)[0] == 2);
    request(1, snapshot.generation, 0, 1); exchange(13);
    request(1, snapshot.generation, 4367, 1); exchange(13);
    const unsigned invalid[][2] = {{0, 0}, {0, 257}, {4368, 1}, {4367, 2}, {65535, 256}};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        request(1, snapshot.generation, invalid[i][0], invalid[i][1]); written = 99;
        assert(tunuino_map_read_reply(&snapshot, packet, request_length, reply, sizeof(reply), &written) == TUNUINO_MAP_READ_BAD_REQUEST);
        assert(written == 0);
    }
    request(1, 0, 0, 1);
    assert(tunuino_map_read_reply(&snapshot, packet, request_length, reply, sizeof(reply), &written) == TUNUINO_MAP_READ_BAD_REQUEST);
    request(0, 0, 0, 0);
    assert(tunuino_map_read_reply(&snapshot, packet, request_length, reply, 1, &written) == TUNUINO_MAP_READ_CAPACITY && written == 0);
    packet[request_length - 1] ^= 1;
    assert(tunuino_map_read_reply(&snapshot, packet, request_length, reply, sizeof(reply), &written) == TUNUINO_MAP_READ_BAD_FRAME);
    request(0, 0, 0, 0); q[9] = 2;
    assert(tunuino_frame_encode(q, 11, packet, sizeof(packet), &request_length) == TUNUINO_FRAME_OK);
    assert(tunuino_map_read_reply(&snapshot, packet, request_length, reply, sizeof(reply), &written) == TUNUINO_MAP_READ_BAD_REQUEST);
    request(0, 0, 0, 0); q[10] = 3;
    assert(tunuino_frame_encode(q, 11, packet, sizeof(packet), &request_length) == TUNUINO_FRAME_OK);
    assert(tunuino_map_read_reply(&snapshot, packet, request_length, reply, sizeof(reply), &written) == TUNUINO_MAP_READ_BAD_REQUEST);
    request(0, 0, 0, 0); q[10] = 1;
    assert(tunuino_frame_encode(q, 11, packet, sizeof(packet), &request_length) == TUNUINO_FRAME_OK);
    assert(exchange(1)[0] == 1); /* other map kind unavailable */
    request(0, 0, 0, 0); snapshot.bytes[20] ^= 1;
    assert(exchange(1)[0] == 1);
    assert(tunuino_map_snapshot_prepare(&snapshot, &map, 0) == TUNUINO_MAP_ARGUMENT);
    assert(snapshot.length == 0 && snapshot.generation == 0 && exchange(1)[0] == 1);
    map.cells[0][0] = 90001;
    assert(tunuino_map_snapshot_prepare(&snapshot, &map, 2) == TUNUINO_MAP_RANGE);
    assert(snapshot.generation == 0);
    assert(tunuino_map_read_reply(NULL, packet, request_length, reply, sizeof(reply), &written) == TUNUINO_MAP_READ_ARGUMENT && written == 0);
    puts("C read-only map snapshots, chunk bounds, stale generation and corruption checks passed");
    return 0;
}
