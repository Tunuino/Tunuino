/* Copyright (c) 2026 Tunuino. */
#include "tunuino/dispatch.h"
#include <string.h>

tunuino_dispatch_status tunuino_dispatch_validate(const tunuino_dispatch_context *c) {
    if (c == NULL || c->identity == NULL) { return TUNUINO_DISPATCH_ARGUMENT; }
    if (tunuino_identity_validate(c->identity) != TUNUINO_IDENTITY_OK) {
        return TUNUINO_DISPATCH_UNCONFIGURED;
    }
    uint32_t caps = c->identity->capabilities;
    if ((caps & ~(TUNUINO_DISPATCH_LIVE | TUNUINO_DISPATCH_MAP_READ)) != 0
        || ((caps & TUNUINO_DISPATCH_LIVE) != 0 && c->live == NULL)) {
        return TUNUINO_DISPATCH_UNCONFIGURED;
    }
    if ((caps & TUNUINO_DISPATCH_MAP_READ) != 0) {
        if ((c->ve == NULL && c->ignition == NULL)
            || (c->ve != NULL && c->ve->generation != 0 && c->ve->kind != TUNUINO_MAP_VE)
            || (c->ignition != NULL && c->ignition->generation != 0 && c->ignition->kind != TUNUINO_MAP_IGNITION)) {
            return TUNUINO_DISPATCH_UNCONFIGURED;
        }
    }
    return TUNUINO_DISPATCH_OK;
}

tunuino_dispatch_status tunuino_dispatch_reply(const tunuino_dispatch_context *c,
    const uint8_t *request_packet, size_t request_length,
    uint8_t *response_packet, size_t capacity, size_t *written) {
    if (written != NULL) { *written = 0; }
    if (c == NULL || request_packet == NULL || response_packet == NULL || written == NULL) {
        return TUNUINO_DISPATCH_ARGUMENT;
    }
    const uint8_t *q;
    size_t length;
    if (tunuino_frame_decode(request_packet, request_length, &q, &length) != TUNUINO_FRAME_OK) {
        return TUNUINO_DISPATCH_BAD_FRAME;
    }
    tunuino_dispatch_status config = tunuino_dispatch_validate(c);
    if (config != TUNUINO_DISPATCH_OK) { return config; }
    if (length == 8 && memcmp(q, "TUNUINO?", 8) == 0) {
        tunuino_identity_status status = tunuino_identity_reply(c->identity, request_packet,
            request_length, response_packet, capacity, written);
        return status == TUNUINO_IDENTITY_OK ? TUNUINO_DISPATCH_OK
            : status == TUNUINO_IDENTITY_CAPACITY ? TUNUINO_DISPATCH_CAPACITY : TUNUINO_DISPATCH_SOURCE_ERROR;
    }
    if (length == 9 && memcmp(q, "TUNUINO:L", 9) == 0) {
        if ((c->identity->capabilities & TUNUINO_DISPATCH_LIVE) == 0) {
            return TUNUINO_DISPATCH_UNSUPPORTED;
        }
        tunuino_live_status status = tunuino_live_reply(c->live, request_packet,
            request_length, response_packet, capacity, written);
        return status == TUNUINO_LIVE_OK ? TUNUINO_DISPATCH_OK
            : status == TUNUINO_LIVE_CAPACITY ? TUNUINO_DISPATCH_CAPACITY : TUNUINO_DISPATCH_SOURCE_ERROR;
    }
    if (length >= 9 && memcmp(q, "TUNUINO:M", 9) == 0) {
        if ((c->identity->capabilities & TUNUINO_DISPATCH_MAP_READ) == 0) {
            return TUNUINO_DISPATCH_UNSUPPORTED;
        }
        const tunuino_map_snapshot *snapshot = length >= 11 && q[10] == TUNUINO_MAP_IGNITION
            ? c->ignition : c->ve;
        /* A snapshot of the other kind yields NOT_READY for a missing map.
         * The map handler still validates the entire request before replying. */
        if (snapshot == NULL) { snapshot = c->ve != NULL ? c->ve : c->ignition; }
        tunuino_map_read_status status = tunuino_map_read_reply(snapshot, request_packet,
            request_length, response_packet, capacity, written);
        return status == TUNUINO_MAP_READ_OK ? TUNUINO_DISPATCH_OK
            : status == TUNUINO_MAP_READ_CAPACITY ? TUNUINO_DISPATCH_CAPACITY
            : status == TUNUINO_MAP_READ_BAD_REQUEST ? TUNUINO_DISPATCH_BAD_REQUEST : TUNUINO_DISPATCH_SOURCE_ERROR;
    }
    return TUNUINO_DISPATCH_UNKNOWN_REQUEST;
}
