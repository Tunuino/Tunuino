// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#ifndef TUNUINO_STM32F429_BOARD_H
#define TUNUINO_STM32F429_BOARD_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TUNUINO_BOARD_NAME "Tunuino STM32F429ZI"
#define TUNUINO_INJECTOR_COUNT 8u
#define TUNUINO_IGNITION_COUNT 8u
#define TUNUINO_CAN_WIDEBAND_BUS 1u
// Proteus F4 reference assignments; these are MCU pins, not connector proof.
#define TUNUINO_CONSOLE_USART 2u
#define TUNUINO_CONSOLE_TX_PORT 'D'
#define TUNUINO_CONSOLE_TX_PIN 5u
#define TUNUINO_CONSOLE_RX_PORT 'D'
#define TUNUINO_CONSOLE_RX_PIN 6u
#define TUNUINO_CONSOLE_AF 7u
#define TUNUINO_CAN1_RX_PORT 'D'
#define TUNUINO_CAN1_RX_PIN 0u
#define TUNUINO_CAN1_TX_PORT 'D'
#define TUNUINO_CAN1_TX_PIN 1u
#define TUNUINO_CAN1_AF 9u
// No GPIO mux, transceiver enable or CAN transmission is performed.
typedef enum {
    TUNUINO_HW_UNINITIALIZED = 0,
    TUNUINO_HW_SAFE = 1,
    TUNUINO_HW_FAULT = 2
} tunuino_hw_state_t;
// All power outputs must remain disabled until validated and explicitly armed.
// This header intentionally does not assign provisional pins.
typedef struct {
    tunuino_hw_state_t state;
    uint8_t outputs_armed;
} tunuino_board_status_t;
void tunuino_board_reset_status(tunuino_board_status_t *status);
uint8_t tunuino_board_outputs_allowed(const tunuino_board_status_t *status);
#ifdef __cplusplus
}
#endif
#endif
