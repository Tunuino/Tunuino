// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "tunerstudio_serial.h"
void tunuino_ts_serial_init(tunuino_ts_serial_t *serial, tunuino_ts_write_fn write,
                             void *context) {
    if (!serial) return;
    *serial = (tunuino_ts_serial_t){0};
    serial->write = write;
    serial->context = context;
}
uint8_t tunuino_ts_serial_receive(tunuino_ts_serial_t *serial, uint8_t command,
                                   const tunuino_ts_values_t *values) {
    if (!serial) return 0u;
    // Explicitly reject all commands except the two read-only operations.
    if (command != (uint8_t)'S' && command != (uint8_t)'A') {
        ++serial->commands_rejected;
        return 0u;
    }
    uint8_t response[32];
    const size_t length = tunuino_ts_reply(command, values, response, sizeof(response));
    if (!length || !serial->write) {
        ++serial->responses_failed;
        return 0u;
    }
    // The callback must either accept the whole reply or report failure.
    // Partial transmission requires a separate buffered transport layer.
    if (serial->write(response, length, serial->context) != length) {
        ++serial->responses_failed;
        return 0u;
    }
    ++serial->commands_handled;
    return 1u;
}
