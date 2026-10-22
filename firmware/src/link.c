/* Copyright (c) 2026 Tunuino. */
#include "tunuino/link.h"
#include <string.h>
_Static_assert(TUNUINO_DISPATCH_RESPONSE_MAX >= TUNUINO_IDENTITY_BODY_SIZE + TUNUINO_FRAME_OVERHEAD,
    "Response buffer must fit identity");
_Static_assert(TUNUINO_DISPATCH_RESPONSE_MAX >= TUNUINO_LIVE_BODY_SIZE + TUNUINO_FRAME_OVERHEAD,
    "Response buffer must fit live data");

static tunuino_link_status fail(tunuino_link *link, tunuino_link_status status) {
    tunuino_rx_abort(&link->rx);
    tunuino_tx_abort(&link->tx);
    link->state = status;
    return status;
}
tunuino_link_status tunuino_link_init(tunuino_link *link, const tunuino_dispatch_context *context,
    uint32_t rx_timeout_ms, uint32_t tx_timeout_ms, uint64_t now_ms) {
    if (link == NULL) { return TUNUINO_LINK_ARGUMENT; }
    memset(link, 0, sizeof(*link));
    link->state = TUNUINO_LINK_ARGUMENT;
    if (context == NULL) { return link->state; }
    link->context = *context;
    link->dispatch_status = tunuino_dispatch_validate(&link->context);
    if (link->dispatch_status != TUNUINO_DISPATCH_OK) { return fail(link, TUNUINO_LINK_DISPATCH_ERROR); }
    link->rx_status = tunuino_rx_init(&link->rx, rx_timeout_ms, now_ms);
    link->tx_status = tunuino_tx_init(&link->tx, tx_timeout_ms, now_ms);
    if (link->rx_status != TUNUINO_RX_MORE || link->tx_status != TUNUINO_TX_IDLE) {
        return fail(link, TUNUINO_LINK_ARGUMENT);
    }
    link->state = TUNUINO_LINK_RECEIVING;
    return link->state;
}
tunuino_link_status tunuino_link_poll(tunuino_link *link, uint64_t now_ms) {
    if (link == NULL) { return TUNUINO_LINK_ARGUMENT; }
    if (link->state == TUNUINO_LINK_RECEIVING) {
        link->rx_status = tunuino_rx_poll(&link->rx, now_ms);
        if (link->rx_status != TUNUINO_RX_MORE) { return fail(link, TUNUINO_LINK_RX_ERROR); }
    } else if (link->state == TUNUINO_LINK_RESPONDING) {
        link->tx_status = tunuino_tx_poll(&link->tx, now_ms);
        if (link->tx_status != TUNUINO_TX_PENDING) { return fail(link, TUNUINO_LINK_TX_ERROR); }
    }
    return link->state;
}
tunuino_link_status tunuino_link_feed(tunuino_link *link, const uint8_t *bytes,
    size_t length, uint64_t now_ms, size_t *consumed) {
    if (consumed != NULL) { *consumed = 0; }
    if (link == NULL || consumed == NULL || (length != 0 && bytes == NULL)) { return TUNUINO_LINK_ARGUMENT; }
    if (tunuino_link_poll(link, now_ms) != TUNUINO_LINK_RECEIVING) { return link->state; }
    link->rx_status = tunuino_rx_feed(&link->rx, bytes, length, now_ms, consumed);
    if (link->rx_status == TUNUINO_RX_MORE) { return link->state; }
    if (link->rx_status != TUNUINO_RX_READY) { return fail(link, TUNUINO_LINK_RX_ERROR); }
    const uint8_t *packet;
    size_t packet_length;
    link->rx_status = tunuino_rx_view(&link->rx, &packet, &packet_length);
    if (link->rx_status != TUNUINO_RX_READY) { return fail(link, TUNUINO_LINK_RX_ERROR); }
    uint8_t response[TUNUINO_DISPATCH_RESPONSE_MAX];
    size_t written;
    link->dispatch_status = tunuino_dispatch_reply(&link->context, packet, packet_length,
        response, sizeof(response), &written);
    if (link->dispatch_status != TUNUINO_DISPATCH_OK) { return fail(link, TUNUINO_LINK_DISPATCH_ERROR); }
    link->tx_status = tunuino_tx_begin(&link->tx, response, written, now_ms);
    if (link->tx_status != TUNUINO_TX_PENDING) { return fail(link, TUNUINO_LINK_TX_ERROR); }
    link->state = TUNUINO_LINK_RESPONDING;
    return link->state;
}
tunuino_link_status tunuino_link_tx_view(tunuino_link *link, uint64_t now_ms,
    const uint8_t **bytes, size_t *remaining) {
    if (bytes != NULL) { *bytes = NULL; }
    if (remaining != NULL) { *remaining = 0; }
    if (link == NULL || bytes == NULL || remaining == NULL) { return TUNUINO_LINK_ARGUMENT; }
    if (tunuino_link_poll(link, now_ms) != TUNUINO_LINK_RESPONDING) { return link->state; }
    link->tx_status = tunuino_tx_view(&link->tx, now_ms, bytes, remaining);
    if (link->tx_status != TUNUINO_TX_PENDING) { return fail(link, TUNUINO_LINK_TX_ERROR); }
    return link->state;
}
tunuino_link_status tunuino_link_tx_advance(tunuino_link *link, size_t completed, uint64_t now_ms) {
    if (link == NULL) { return TUNUINO_LINK_ARGUMENT; }
    if (tunuino_link_poll(link, now_ms) != TUNUINO_LINK_RESPONDING) {
        return link->state == TUNUINO_LINK_RECEIVING ? TUNUINO_LINK_ARGUMENT : link->state;
    }
    link->tx_status = tunuino_tx_advance(&link->tx, completed, now_ms);
    if (link->tx_status == TUNUINO_TX_PENDING) { return link->state; }
    if (link->tx_status != TUNUINO_TX_DONE) { return fail(link, TUNUINO_LINK_TX_ERROR); }
    link->rx_status = tunuino_rx_release(&link->rx, now_ms);
    if (link->rx_status != TUNUINO_RX_MORE) { return fail(link, TUNUINO_LINK_RX_ERROR); }
    link->state = TUNUINO_LINK_RECEIVING;
    return link->state;
}
void tunuino_link_abort(tunuino_link *link) {
    if (link != NULL) { (void)fail(link, TUNUINO_LINK_CLOSED); }
}
