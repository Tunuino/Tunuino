/* Copyright (c) 2026 Tunuino. Simulated USB callbacks around the real bench loop.
 * Does not test STM32 registers, TinyUSB internals or physical USB enumeration. */
#include "tusb.h"
#include "tunuino/identity.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
int tunuino_usb_bench_main(void);
void tud_mount_cb(void);
void tud_cdc_line_state_cb(uint8_t interface, bool dtr, bool rts);
void tud_cdc_tx_complete_cb(uint8_t interface);
volatile uint32_t tunuino_bench_fault;
static jmp_buf finish;
static const char *mode;
static uint64_t now;
static uint8_t input[64], transfer[64], wire[128];
static size_t input_length, transfer_length, wire_length;
static unsigned iteration, writes;
static bool connected;
void board_init(void) {}
void board_uid(uint8_t uid[12]) { for (unsigned i = 0; i < 12; ++i) uid[i] = (uint8_t)(i + 1); }
uint64_t board_millis(void) { return now; }
bool tusb_init(uint8_t port, const tusb_rhport_init_t *config) {
    assert(port == 0 && config->role == TUSB_ROLE_DEVICE && config->speed == TUSB_SPEED_FULL);
    return true;
}
bool tud_cdc_connected(void) { return connected; }
uint32_t tud_cdc_read(void *buffer, uint32_t capacity) {
    assert(input_length <= capacity);
    memcpy(buffer,input,input_length);
    uint32_t length = (uint32_t)input_length;
    input_length = 0;
    return length;
}
uint32_t tud_cdc_write(const void *buffer, uint32_t length) {
    assert(transfer_length == 0 && length > 0 && length <= 63);
    if (length > 5) length = 5; /* Exercise partial FIFO acceptance. */
    memcpy(transfer,buffer,length); transfer_length = length;
    ++writes;
    return length;
}
uint32_t tud_cdc_write_flush(void) { return (uint32_t)transfer_length; }
void tud_task(void) {
    ++iteration; ++now;
    assert(iteration < 2000);
    if (iteration == 1) {
        tud_mount_cb(); connected = true;
        tud_cdc_line_state_cb(0,true,false);
        /* The first/coalesced requests are already buffered with the DTR event. */
    }
    if (strcmp(mode,"normal") == 0) {
        if (transfer_length) {
            assert(wire_length + transfer_length <= sizeof(wire));
            memcpy(wire + wire_length,transfer,transfer_length);
            wire_length += transfer_length; transfer_length = 0;
            tud_cdc_tx_complete_cb(0);
        }
        if (wire_length == 2 * (TUNUINO_IDENTITY_BODY_SIZE + TUNUINO_FRAME_OVERHEAD)) longjmp(finish,1);
    } else if (strcmp(mode,"close") == 0) {
        if (iteration == 5) { connected = false; tud_cdc_line_state_cb(0,false,false); }
        if (iteration == 7) { connected = true; tud_cdc_line_state_cb(0,true,false); }
        if (iteration == 12) { assert(writes == 1); longjmp(finish,1); }
    } else if (tunuino_bench_fault != 0) {
        assert(tunuino_bench_fault == 20 && writes == 0);
        if (strcmp(mode,"timeout") == 0) assert(now >= 1000);
        longjmp(finish,1);
    }
}
int main(int argc, char **argv) {
    assert(argc == 2); mode = argv[1];
    size_t length;
    assert(tunuino_frame_encode((const uint8_t *)"TUNUINO?",8,input,sizeof(input),&length) == TUNUINO_FRAME_OK);
    input_length = length;
    if (strcmp(mode,"normal") == 0) {
        memcpy(input + length,input,length); input_length *= 2;
    } else if (strcmp(mode,"timeout") == 0) input_length = 3;
    else if (strcmp(mode,"corrupt") == 0) input[length - 1] ^= 1;
    else assert(strcmp(mode,"close") == 0);
    if (setjmp(finish) == 0) { (void)tunuino_usb_bench_main(); assert(0); }
    if (strcmp(mode,"normal") == 0) {
        for (unsigned i = 0; i < 2; ++i) {
            const uint8_t *body; size_t count;
            assert(tunuino_frame_decode(wire + i * 38,38,&body,&count) == TUNUINO_FRAME_OK);
            assert(count == 32 && body[9] == TUNUINO_PROTEUS && body[10] == TUNUINO_STM32_F4
                && body[11] == 0 && body[12] == 7 && body[15] == 1);
            for (unsigned c = 16; c < 20; ++c) assert(body[c] == 0);
            for (unsigned c = 0; c < 12; ++c) assert(body[20 + c] == c + 1);
        }
        assert(writes > 2 && tunuino_bench_fault == 0);
    }
    printf("C USB bench callback simulation: %s passed\n",mode);
    return 0;
}
