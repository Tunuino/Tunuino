// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "../firmware/console/flash_target.h"
#include <assert.h>
int main(void){
 tunuino_flash_target_t x={429u,429u,1u,1u,TUNUINO_FLASH_STM32_DFU,TUNUINO_FW_HEX};
 assert(tunuino_flash_target_ready(&x));
 x.firmware_board_id=407u;assert(!tunuino_flash_target_ready(&x));
 x.firmware_board_id=429u;x.bootloader_verified=0u;assert(!tunuino_flash_target_ready(&x));
 x.bootloader_verified=1u;x.format=TUNUINO_FW_BIN;assert(!tunuino_flash_target_ready(&x));
 x.method=TUNUINO_FLASH_STLINK;assert(tunuino_flash_target_ready(&x));
 x.method=TUNUINO_FLASH_OPENBLT;assert(!tunuino_flash_target_ready(&x));
 return 0;
}
