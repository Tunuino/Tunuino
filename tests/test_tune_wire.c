// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/tune_wire.h"
#include <assert.h>
#include <string.h>
int main(void) {
    tunuino_tune_maps_t maps = {0};
    for (unsigned t = 0u; t < 2u; ++t) {
        tunuino_tune_map_t *m = t ? &maps.ignition : &maps.ve;
        for (unsigned i = 0u; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
            m->rpm_axis[i] = (uint16_t)(1000u + 500u * i);
            m->load_axis[i] = (uint16_t)(200u + 100u * i);
            for (unsigned j = 0u; j < TUNUINO_MAP_AXIS_SIZE; ++j)
                m->cells[i][j] = t ? -100 : 850;
        }
    }
    tunuino_tune_store_t store = {0};
    assert(tunuino_tune_store_init(&store, &maps) == TUNUINO_TUNE_OK);
    tunuino_tune_wire_t wire = {0};
    assert(tunuino_tune_wire_build(&wire, &store));
    assert(wire.bytes[0] == 'T' && wire.bytes[3] == 'E');
    assert(wire.bytes[4] == 1u && wire.bytes[6] == 0x50u);
    uint8_t collected[TUNUINO_TUNE_WIRE_SIZE] = {0};
    size_t at = 0u;
    while (at < sizeof(collected)) {
        const size_t n = tunuino_tune_wire_read(&wire, at,
            collected + at, sizeof(collected) - at);
        assert(n > 0u && n <= TUNUINO_TUNE_WIRE_CHUNK_MAX);
        at += n;
    }
    assert(memcmp(collected, wire.bytes, sizeof(collected)) == 0);
    assert(tunuino_tune_wire_crc32(collected, sizeof(collected) - 4u) == wire.crc32);
    assert(!tunuino_tune_wire_read(&wire, sizeof(collected), collected, 10u));
    collected[50] ^= 1u;
    assert(tunuino_tune_wire_crc32(collected, sizeof(collected) - 4u) != wire.crc32);
    return 0;
}
