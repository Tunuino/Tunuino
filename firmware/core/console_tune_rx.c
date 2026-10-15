// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "console_tune_rx.h"
#include <string.h>
void tunuino_console_tune_rx_init(tunuino_console_tune_rx_t *rx) {
    if (rx) memset(rx, 0, sizeof(*rx));
}
void tunuino_console_tune_rx_byte(tunuino_console_tune_rx_t *rx,
    uint8_t byte, const tunuino_tune_wire_t *wire, tunuino_tx_queue_t *queue) {
    if (!rx) return;
    if (rx->length == 0u) {
        if (byte == 0x54u) { rx->frame[0] = byte; rx->length = 1u; }
        return;
    }
    if (rx->length == 1u && byte != 0x4eu) {
        rx->length = 0u;
        if (byte == 0x54u) { rx->frame[0] = byte; rx->length = 1u; }
        return;
    }
    rx->frame[rx->length++] = byte;
    if (rx->length == sizeof(rx->frame)) {
        if (tunuino_console_tune_handle_frame(wire, rx->frame, sizeof(rx->frame), queue))
            rx->accepted++;
        else
            rx->rejected++;
        rx->length = 0u;
    }
}
