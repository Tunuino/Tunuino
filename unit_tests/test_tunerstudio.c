/* Exercise the same dispatcher/link used by the board; no physical ECU. */
#include "tunuino/link.h"
#include "tunuino/bench_tunerstudio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static tunuino_identity identity = {TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,
    TUNUINO_BENCH_MAJOR,TUNUINO_BENCH_MINOR,TUNUINO_BENCH_PATCH,0,
    {1,2,3,4,5,6,7,8,9,10,11,12}};
static const tunuino_dispatch_context context = {&identity,NULL,NULL,NULL,NULL};
static tunuino_link link;
static uint8_t output[300];
static size_t output_size;

static void init(void) {
    assert(tunuino_link_init(&link,&context,1000,1000,0) == TUNUINO_LINK_RECEIVING);
}
static void request(const uint8_t *bytes,size_t length) {
    for (size_t i=0;i<length;++i) {
        size_t used;
        assert(tunuino_link_feed(&link,bytes+i,1,0,&used) ==
            (i+1 == length ? TUNUINO_LINK_RESPONDING : TUNUINO_LINK_RECEIVING));
        assert(used == 1);
    }
    /* A queued response must finish before another request is consumed. */
    size_t used;
    assert(tunuino_link_feed(&link,(const uint8_t *)"Q",1,0,&used) == TUNUINO_LINK_RESPONDING && used == 0);
    output_size = 0;
    while (link.state == TUNUINO_LINK_RESPONDING) {
        const uint8_t *view;
        size_t remaining;
        assert(tunuino_link_tx_view(&link,0,&view,&remaining) == TUNUINO_LINK_RESPONDING);
        size_t count = remaining > 3 ? 3 : remaining;
        assert(output_size+count <= sizeof(output));
        memcpy(output+output_size,view,count); output_size += count;
        assert(tunuino_link_tx_advance(&link,count,0) ==
            (count == remaining ? TUNUINO_LINK_RECEIVING : TUNUINO_LINK_RESPONDING));
    }
}
static void framed(const char *body) {
    uint8_t packet[128]; size_t count;
    assert(tunuino_frame_encode((const uint8_t *)body,strlen(body),packet,sizeof(packet),&count) == TUNUINO_FRAME_OK);
    request(packet,count);
}
static void vector(const char *name) {
    printf("%s ",name);
    for (size_t i=0;i<output_size;++i) printf("%02x",output[i]);
    puts("");
}
int main(void) {
    init();
    request((const uint8_t *)"F",1); vector("plain_f");
    request((const uint8_t *)"Q",1); vector("plain_q");
    request((const uint8_t *)"S",1); vector("plain_s");
    framed("S"); vector("s");
    framed("V"); vector("v");
    framed("A"); vector("a");
    framed("R"); vector("r");
    framed("TUNUINO?"); vector("native");
    tunuino_identity before = identity;
    for (const char *commands="BCWbw";*commands;++commands) {
        char body[]={*commands,'F','Q','S',0};
        framed(body);
        const uint8_t *reply; size_t size;
        assert(tunuino_frame_decode(output,output_size,&reply,&size) == TUNUINO_FRAME_OK);
        assert(size == 1 && reply[0] == 0x83);
        assert(memcmp(&before,&identity,sizeof(identity)) == 0);
    }
    /* A read remains possible after a rejected write/burn. */
    framed("A");
    /* Coalesced probes and a framed request retain their unconsumed suffix. */
    uint8_t combined[64]={'F','Q'}; size_t count,used;
    assert(tunuino_frame_encode((const uint8_t *)"A",1,combined+2,sizeof(combined)-2,&count) == TUNUINO_FRAME_OK);
    for (size_t cursor=0;cursor<count+2;) {
        assert(tunuino_link_feed(&link,combined+cursor,count+2-cursor,0,&used) == TUNUINO_LINK_RESPONDING);
        assert(used == (cursor<2 ? 1 : count)); cursor += used;
        const uint8_t *view; size_t remaining;
        assert(tunuino_link_tx_view(&link,0,&view,&remaining) == TUNUINO_LINK_RESPONDING);
        assert(tunuino_link_tx_advance(&link,remaining,0) == TUNUINO_LINK_RECEIVING);
    }
    /* Never interpret an ASCII probe as one when it occurs inside a frame. */
    const uint8_t embedded[]={0,'Q','S','F'};
    uint8_t packet[128];
    assert(tunuino_frame_encode(embedded,sizeof(embedded),packet,sizeof(packet),&count) == TUNUINO_FRAME_OK);
    request(packet,count);
    const uint8_t *reply; size_t size;
    assert(tunuino_frame_decode(output,output_size,&reply,&size) == TUNUINO_FRAME_OK && size == 1 && reply[0] == 0x83);
    /* Both discovery forms reject insufficient output capacity without writes. */
    for (size_t cap=0;cap<sizeof(TUNUINO_TS_SIGNATURE);++cap) {
        memset(output,0xA5,sizeof(output)); output_size=999;
        assert(tunuino_dispatch_plain_reply(&context,'Q',output,cap,&output_size) == TUNUINO_DISPATCH_CAPACITY);
        assert(output_size == 0 && output[0] == 0xA5);
    }
    assert(tunuino_frame_encode((const uint8_t *)"S",1,packet,sizeof(packet),&count) == TUNUINO_FRAME_OK);
    for (size_t cap=0;cap<1+sizeof(TUNUINO_TS_SIGNATURE)+TUNUINO_FRAME_OVERHEAD;++cap) {
        memset(output,0xA5,sizeof(output)); output_size=999;
        assert(tunuino_dispatch_reply(&context,packet,count,output,cap,&output_size) == TUNUINO_DISPATCH_CAPACITY);
        assert(output_size == 0 && output[0] == 0xA5);
    }
    /* No discovery for an old image or a different board/profile. */
    identity.firmware_patch=2;
    assert(tunuino_dispatch_plain_reply(&context,'Q',output,sizeof(output),&output_size) == TUNUINO_DISPATCH_UNKNOWN_REQUEST);
    assert(tunuino_dispatch_reply(&context,packet,count,output,sizeof(output),&output_size) == TUNUINO_DISPATCH_UNKNOWN_REQUEST);
    identity=before; identity.hardware_revision=8;
    assert(tunuino_dispatch_plain_reply(&context,'Q',output,sizeof(output),&output_size) == TUNUINO_DISPATCH_UNKNOWN_REQUEST);
    identity=before;
    /* Raw output shares the same bounds and total TX deadline. */
    tunuino_stream_tx tx;
    assert(tunuino_tx_init(&tx,10,0) == TUNUINO_TX_IDLE);
    assert(tunuino_tx_begin_plain(&tx,output,0,0) == TUNUINO_TX_ARGUMENT);
    assert(tunuino_tx_begin_plain(&tx,output,sizeof(tx.packet)+1,0) == TUNUINO_TX_ARGUMENT);
    assert(tunuino_tx_begin_plain(&tx,(const uint8_t *)"001",3,0) == TUNUINO_TX_PENDING);
    assert(tunuino_tx_begin_plain(&tx,(const uint8_t *)"001",3,0) == TUNUINO_TX_BUSY);
    assert(tunuino_tx_poll(&tx,10) == TUNUINO_TX_TIMEOUT);
    /* Probes cannot bypass an incomplete or CRC-corrupted frame. */
    init();
    assert(tunuino_link_feed(&link,packet,1,0,&used) == TUNUINO_LINK_RECEIVING && used == 1);
    assert(tunuino_link_feed(&link,(const uint8_t *)"Q",1,0,&used) == TUNUINO_LINK_RECEIVING);
    assert(tunuino_link_poll(&link,1000) == TUNUINO_LINK_RX_ERROR);
    init(); packet[count-1]^=1;
    assert(tunuino_link_feed(&link,packet,count,0,&used) == TUNUINO_LINK_RX_ERROR);
    assert(tunuino_link_feed(&link,(const uint8_t *)"Q",1,0,&used) == TUNUINO_LINK_RX_ERROR && used == 0);
    puts("OK");
    return 0;
}
