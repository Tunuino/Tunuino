// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_tune_link.h"
#include <assert.h>
#include <string.h>
typedef struct { uint8_t bytes[2048]; size_t count; } sink_t;
static uint8_t ready(void *ctx) { (void)ctx; return 1u; }
static void send(uint8_t byte, void *ctx) {
    sink_t *s = (sink_t *)ctx;
    assert(s->count < sizeof(s->bytes));
    s->bytes[s->count++] = byte;
}
int main(void) {
    tunuino_tune_maps_t maps = {0};
    for (unsigned t = 0; t < 2u; ++t) {
        tunuino_tune_map_t *m = t ? &maps.ignition : &maps.ve;
        for (unsigned i = 0; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
            m->rpm_axis[i] = (uint16_t)(1000u + i * 500u);
            m->load_axis[i] = (uint16_t)(200u + i * 100u);
            for (unsigned j = 0; j < TUNUINO_MAP_AXIS_SIZE; ++j)
                m->cells[i][j] = t ? 120 : 850;
        }
    }
    tunuino_tune_store_t store = {0};
    tunuino_tune_wire_t wire = {0};
    assert(tunuino_tune_store_init(&store, &maps) == TUNUINO_TUNE_OK);
    assert(tunuino_tune_wire_build(&wire, &store));
    sink_t sink = {0};
    tunuino_console_tune_link_t link;
    tunuino_console_tune_link_init(&link, &wire, ready, send, &sink);
    uint8_t reconstructed[TUNUINO_TUNE_WIRE_SIZE] = {0};
    for (size_t offset = 0; offset < TUNUINO_TUNE_WIRE_SIZE; offset += 32u) {
        const uint8_t count = (uint8_t)((TUNUINO_TUNE_WIRE_SIZE - offset) < 32u ?
            TUNUINO_TUNE_WIRE_SIZE - offset : 32u);
        const uint8_t payload[3] = {(uint8_t)offset, (uint8_t)(offset >> 8), count};
        uint8_t request[11];
        assert(tunuino_console_encode(5u, payload, sizeof(payload), request, sizeof(request)) == 11u);
        for (size_t i = 0; i < sizeof(request); ++i)
            tunuino_console_tune_link_rx(&link, request[i]);
        assert(link.rx.accepted == (uint32_t)(offset / 32u + 1u));
        while (tunuino_console_tune_link_step(&link)) {}
        assert(sink.count == (size_t)count + 11u);
        assert(sink.bytes[0] == 0x54u && sink.bytes[1] == 0x4eu);
        assert(sink.bytes[3] == 4u && sink.bytes[6] == payload[0] &&
            sink.bytes[7] == payload[1] && sink.bytes[8] == count);
        assert(sink.bytes[4] == (uint8_t)(count + 3u) && sink.bytes[5] == 0u);
        const uint16_t crc = tunuino_console_crc16(sink.bytes + 2u, (size_t)count + 7u);
        assert(sink.bytes[count + 9u] == (uint8_t)crc);
        assert(sink.bytes[count + 10u] == (uint8_t)(crc >> 8u));
        memcpy(reconstructed + offset, sink.bytes + 9u, count);
        sink.count = 0u;
    }
    assert(link.rx.accepted == 37u);
    assert(memcmp(reconstructed, wire.bytes, sizeof(reconstructed)) == 0);
    assert(tunuino_tune_wire_crc32(reconstructed, sizeof(reconstructed) - 4u) ==
        ((uint32_t)reconstructed[1164] | ((uint32_t)reconstructed[1165] << 8u) |
        ((uint32_t)reconstructed[1166] << 16u) | ((uint32_t)reconstructed[1167] << 24u)));
    return 0;
}
