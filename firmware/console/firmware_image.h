// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#ifndef TUNUINO_FIRMWARE_IMAGE_H
#define TUNUINO_FIRMWARE_IMAGE_H
#include <stddef.h>
#include <stdint.h>
typedef enum { TUNUINO_FW_UNKNOWN=0, TUNUINO_FW_BIN=1, TUNUINO_FW_HEX=2 } tunuino_fw_format_t;
// File recognition only. Flashing requires a target-specific verified bootloader.
tunuino_fw_format_t tunuino_firmware_detect(const uint8_t *data, size_t length);
uint8_t tunuino_firmware_intel_hex_check(const uint8_t *data, size_t length);
#endif
