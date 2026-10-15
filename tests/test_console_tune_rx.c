// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_tune_rx.h"
#include <assert.h>
int main(void) {
    tunuino_tune_maps_t maps = {0};
    for (unsigned t = 0; t < 2u; ++t) {
        tunuino_tune_map_t *m = t ? &maps.ignition : &maps.ve;
        for (unsigned i = 0; i < TUNUINO_MAP_AXIS_SIZE; ++i) {
            m->rpm_axis[i] = (uint16_t)(1000u + 500u * i);
            m->load_axis[i] = (uint16_t)(200u + 100u * i);
            for (unsigned j = 0; j < TUNUINO_MAP_AXIS_SIZE; ++j)
                m->cells[i][j] = t ? 120 : 850;
        }
    }
    tunuino_tune_store_t store = {0};
    tunuino_tune_wire_t wire = {0};
    tunuino_tx_queue_t queue = {0};
    tunuino_console_tune_rx_t rx;
    tunuino_console_tune_rx_init(&rx);
    tunuino_tx_queue_init(&queue);
    assert(tunuino_tune_store_init(&store, &maps) == TUNUINO_TUNE_OK);
    assert(tunuino_tune_wire_build(&wire, &store));
    const uint8_t payload[] = {0u, 0u, 32u};
    uint8_t frame[16];
    const size_t size = tunuino_console_encode(5u, payload, sizeof(payload), frame, sizeof(frame));
    assert(size == 11u);
    tunuino_console_tune_rx_byte(&rx, 0x11u, &wire, &queue);
    for (size_t i = 0; i < size; ++i)
        tunuino_console_tune_rx_byte(&rx, frame[i], &wire, &queue);
    assert(rx.accepted == 1u && rx.rejected == 0u);
    assert(tunuino_tx_queue_size(&queue) == 43u);
    frame[9] ^= 1u;
    for (size_t i = 0; i < size; ++i)
        tunuino_console_tune_rx_byte(&rx, frame[i], &wire, &queue);
    assert(rx.accepted == 1u && rx.rejected == 1u);
    assert(tunuino_tx_queue_size(&queue) == 43u);
    return 0;
}
