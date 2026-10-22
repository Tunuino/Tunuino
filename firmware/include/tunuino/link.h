/* Copyright (c) 2026 Tunuino. */
#ifndef TUNUINO_LINK_H
#define TUNUINO_LINK_H
#include "tunuino/dispatch.h"
#include "tunuino/stream_rx.h"
#include "tunuino/stream_tx.h"
typedef enum {
    TUNUINO_LINK_RECEIVING = 0, TUNUINO_LINK_RESPONDING,
    TUNUINO_LINK_ARGUMENT, TUNUINO_LINK_RX_ERROR,
    TUNUINO_LINK_DISPATCH_ERROR, TUNUINO_LINK_TX_ERROR, TUNUINO_LINK_CLOSED
} tunuino_link_status;
/* Caller-owned, serialized operations. Initialize before use. Fields private
 * except state/rx_status/dispatch_status/tx_status (read-only diagnostics).
 * Context pointers must stay valid; synchronize providers during feed/dispatch.
 * No arguments or input/output buffers may overlap this object. */
typedef struct {
    tunuino_stream_rx rx;
    tunuino_stream_tx tx;
    tunuino_dispatch_context context;
    tunuino_link_status state;
    tunuino_rx_status rx_status;
    tunuino_dispatch_status dispatch_status;
    tunuino_tx_status tx_status;
} tunuino_link;
/* Only on a fresh/clean transport, after canceling/draining old driver use. */
tunuino_link_status tunuino_link_init(tunuino_link *link, const tunuino_dispatch_context *context,
    uint32_t rx_timeout_ms, uint32_t tx_timeout_ms, uint64_t now_ms);
/* Consumes at most one request. While responding, consumes zero bytes. Caller
 * retains remaining input until the response is completely transmitted. */
tunuino_link_status tunuino_link_feed(tunuino_link *link, const uint8_t *bytes,
    size_t length, uint64_t now_ms, size_t *consumed);
tunuino_link_status tunuino_link_poll(tunuino_link *link, uint64_t now_ms);
/* At most one driver transfer outstanding; advance only on its completion. */
tunuino_link_status tunuino_link_tx_view(tunuino_link *link, uint64_t now_ms,
    const uint8_t **bytes, size_t *remaining);
tunuino_link_status tunuino_link_tx_advance(tunuino_link *link, size_t completed, uint64_t now_ms);
/* Stops both directions; does not cancel driver operations by itself. */
void tunuino_link_abort(tunuino_link *link);
#endif
