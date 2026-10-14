// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_tune_read.h"
#include <assert.h>
#include <string.h>
int main(void) {
    tunuino_tune_maps_t maps = {0};
    for (unsigned t = 0; t < 2u; ++t) {
        tunuino_tune_map_t *m = t ? &maps.ignition : &maps.ve;
        for (unsigned i = 0; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
            m->rpm_axis[i] = (uint16_t)(1000u + 500u * i);
            m->load_axis[i] = (uint16_t)(200u + 100u * i);
            for (unsigned j = 0; j < TUNUINO_MAP_AXIS_SIZE; ++j)
                m->cells[i][j] = t ? 100 : 800;
        }
    }
    tunuino_tune_store_t store = {0};
    tunuino_tune_wire_t wire = {0};
    assert(tunuino_tune_store_init(&store, &maps) == TUNUINO_TUNE_OK);
    assert(tunuino_tune_wire_build(&wire, &store));
    uint8_t frame[64];
    const uint8_t req[] = {0u, 0u, 32u};
    const size_t size = tunuino_console_tune_reply(&wire, req, sizeof(req), frame, sizeof(frame));
    assert(size == 43u);
    assert(frame[0] == 0x54u && frame[1] == 0x4eu);
    assert(frame[3] == TUNUINO_CONSOLE_TYPE_TUNE_CHUNK);
    assert(frame[4] == 35u && frame[5] == 0u);
    assert(frame[6] == 0u && frame[7] == 0u && frame[8] == 32u);
    assert(memcmp(frame + 9u, wire.bytes, 32u) == 0);
    assert(!tunuino_console_tune_reply(&wire, (const uint8_t[]){0,0,33}, 3u, frame, sizeof(frame)));
    assert(!tunuino_console_tune_reply(&wire, (const uint8_t[]){0x4f,0x01,2}, 3u, frame, sizeof(frame)));
    assert(!tunuino_console_tune_reply(&wire, req, 2u, frame, sizeof(frame)));
    assert(!tunuino_console_tune_reply(&wire, req, 3u, frame, 42u));
    return 0;
}
