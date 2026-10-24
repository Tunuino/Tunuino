/* Copyright (c) 2026 Tunuino. Host test double; NEVER a hardware USB stack. */
#ifndef TUNUINO_TEST_TUSB_H
#define TUNUINO_TEST_TUSB_H
#include <stdbool.h>
#include <stdint.h>
#define TUSB_ROLE_DEVICE 1
#define TUSB_SPEED_FULL 1
typedef struct { uint8_t role, speed; } tusb_rhport_init_t;
bool tusb_init(uint8_t port, const tusb_rhport_init_t *config);
void tud_task(void);
bool tud_cdc_connected(void);
uint32_t tud_cdc_read(void *buffer, uint32_t capacity);
uint32_t tud_cdc_write(const void *buffer, uint32_t length);
uint32_t tud_cdc_write_flush(void);
#endif
