// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "../firmware/console/firmware_image.h"
#include <assert.h>
#include <string.h>
int main(void){
 const char *valid=":020000040800F2\n:00000001FF\n";
 const char *bad=":020000040800F3\n:00000001FF\n";
 const uint8_t binary[]={0x00,0x20,0x01,0x08};
 assert(tunuino_firmware_detect((const uint8_t*)valid,strlen(valid))==TUNUINO_FW_HEX);
 assert(tunuino_firmware_detect((const uint8_t*)bad,strlen(bad))==TUNUINO_FW_UNKNOWN);
 assert(tunuino_firmware_detect(binary,sizeof(binary))==TUNUINO_FW_BIN);
 assert(tunuino_firmware_detect(0,0)==TUNUINO_FW_UNKNOWN);
 assert(!tunuino_firmware_intel_hex_check((const uint8_t*)":00000001FE",11));
 return 0;
}
