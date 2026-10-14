// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_TUNE_WIRE_H
#define TUNUINO_TUNE_WIRE_H
#include <stddef.h>
#include <stdint.h>
#include "tune_store.h"
#define TUNUINO_TUNE_WIRE_VERSION 1u
#define TUNUINO_TUNE_WIRE_SIZE 548u
#define TUNUINO_TUNE_WIRE_CHUNK_MAX 32u
typedef struct {
    uint8_t bytes[TUNUINO_TUNE_WIRE_SIZE];
    uint32_t revision;
    uint32_t crc32;
    uint8_t valid;
} tunuino_tune_wire_t;
// Deterministic, little-endian serialization of validated tune store.
// 2 maps x (8 rpm axes + 8 load axes + 64 signed cells) x 2 bytes,
// plus 8-byte header and 4-byte CRC = 332 bytes.
uint8_t tunuino_tune_wire_build(tunuino_tune_wire_t *wire,
    const tunuino_tune_store_t *store);
size_t tunuino_tune_wire_read(const tunuino_tune_wire_t *wire,
    size_t offset, uint8_t *out, size_t capacity);
uint32_t tunuino_tune_wire_crc32(const uint8_t *data, size_t size);
#endif
