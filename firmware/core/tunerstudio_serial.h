// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TUNERSTUDIO_SERIAL_H
#define TUNUINO_TUNERSTUDIO_SERIAL_H
#include <stddef.h>
#include <stdint.h>
#include "tunerstudio_protocol.h"
typedef size_t (*tunuino_ts_write_fn)(const uint8_t *bytes, size_t count, void *context);
typedef struct {
    tunuino_ts_write_fn write;
    void *context;
    uint32_t commands_handled;
    uint32_t commands_rejected;
    uint32_t responses_failed;
} tunuino_ts_serial_t;
void tunuino_ts_serial_init(tunuino_ts_serial_t *serial, tunuino_ts_write_fn write,
                             void *context);
// One byte is one command in the experimental ASCII subset.
// Do not attach to an untrusted/shared stream without transport arbitration.
uint8_t tunuino_ts_serial_receive(tunuino_ts_serial_t *serial, uint8_t command,
                                   const tunuino_ts_values_t *values);
#endif
