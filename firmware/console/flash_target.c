// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "flash_target.h"
uint8_t tunuino_flash_target_ready(const tunuino_flash_target_t *t) {
 if(!t||!t->board_id||t->board_id!=t->firmware_board_id||t->bootloader_verified!=1u||t->transport_available!=1u)return 0u;
 switch(t->method){
 case TUNUINO_FLASH_STLINK: return (uint8_t)(t->format==TUNUINO_FW_BIN||t->format==TUNUINO_FW_HEX);
 case TUNUINO_FLASH_STM32_DFU: return (uint8_t)(t->format==TUNUINO_FW_HEX);
 case TUNUINO_FLASH_OPENBLT: return 0u; // No verified OpenBLT package parser/transport yet.
 default:return 0u;
 }
}
