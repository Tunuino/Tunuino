// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "tune_wire.h"
#include <string.h>
static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8u);
}
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4u; ++i) p[i] = (uint8_t)(v >> (i * 8u));
}
uint32_t tunuino_tune_wire_crc32(const uint8_t *data, size_t size) {
    uint32_t crc = UINT32_MAX;
    if (!data && size) return 0u;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned b = 0; b < 8u; ++b)
            crc = (crc >> 1u) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return crc ^ UINT32_MAX;
}
uint8_t tunuino_tune_wire_build(tunuino_tune_wire_t *wire,
    const tunuino_tune_store_t *store) {
    if (!wire || !store) return 0u;
    wire->valid = 0u;
    tunuino_tune_maps_t maps;
    uint32_t revision = 0u;
    if (!tunuino_tune_store_snapshot(store, &maps, &revision)) return 0u;
    size_t at = 0u;
    wire->bytes[at++] = 'T'; wire->bytes[at++] = 'U';
    wire->bytes[at++] = 'N'; wire->bytes[at++] = 'E';
    put16(wire->bytes + at, TUNUINO_TUNE_WIRE_VERSION); at += 2u;
    put16(wire->bytes + at, TUNUINO_TUNE_WIRE_SIZE); at += 2u;
    put32(wire->bytes + at, revision); at += 4u;
    const tunuino_tune_map_t *tables[2] = {&maps.ve, &maps.ignition};
    for (unsigned t = 0u; t < 2u; ++t) {
        const tunuino_tune_map_t *m = tables[t];
        for (unsigned i = 0; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
            put16(wire->bytes + at, m->rpm_axis[i]); at += 2u;
        }
        for (unsigned i = 0; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
            put16(wire->bytes + at, m->load_axis[i]); at += 2u;
        }
        for (unsigned y = 0; y < TUNUINO_MAP_AXIS_SIZE; ++y)
            for (unsigned x = 0; x < TUNUINO_MAP_AXIS_SIZE; ++x) {
                put16(wire->bytes + at, (uint16_t)m->cells[y][x]); at += 2u;
            }
    }
    if (at + 4u != TUNUINO_TUNE_WIRE_SIZE) return 0u;
    wire->crc32 = tunuino_tune_wire_crc32(wire->bytes, at);
    put32(wire->bytes + at, wire->crc32);
    wire->revision = revision;
    wire->valid = 1u;
    return 1u;
}
size_t tunuino_tune_wire_read(const tunuino_tune_wire_t *wire,
    size_t offset, uint8_t *out, size_t capacity) {
    if (!wire || !wire->valid || !out || offset >= TUNUINO_TUNE_WIRE_SIZE)
        return 0u;
    size_t count = TUNUINO_TUNE_WIRE_SIZE - offset;
    if (count > capacity) count = capacity;
    if (count > TUNUINO_TUNE_WIRE_CHUNK_MAX) count = TUNUINO_TUNE_WIRE_CHUNK_MAX;
    memcpy(out, wire->bytes + offset, count);
    return count;
}
