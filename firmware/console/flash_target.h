// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#ifndef TUNUINO_FLASH_TARGET_H
#define TUNUINO_FLASH_TARGET_H
#include <stdint.h>
#include "firmware_image.h"
typedef enum { TUNUINO_FLASH_NONE=0,TUNUINO_FLASH_STLINK=1,TUNUINO_FLASH_STM32_DFU=2,TUNUINO_FLASH_OPENBLT=3 } tunuino_flash_method_t;
typedef struct { uint32_t board_id; uint32_t firmware_board_id; uint8_t bootloader_verified; uint8_t transport_available; tunuino_flash_method_t method; tunuino_fw_format_t format; } tunuino_flash_target_t;
// Fail-closed eligibility check only; never starts a flash operation.
uint8_t tunuino_flash_target_ready(const tunuino_flash_target_t *target);
#endif
