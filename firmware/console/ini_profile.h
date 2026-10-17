// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#ifndef TUNUINO_INI_PROFILE_H
#define TUNUINO_INI_PROFILE_H
#include <stddef.h>
#include <stdint.h>
#define TUNUINO_INI_SIGNATURE_MAX 96u
typedef struct { char signature[TUNUINO_INI_SIGNATURE_MAX]; char query_command[24]; uint8_t signature_found; uint8_t query_found; } tunuino_ini_profile_t;
// Read-only subset of TunerStudio INI metadata. No ECU writes or firmware flashing.
uint8_t tunuino_ini_profile_parse(const char *data, size_t length, tunuino_ini_profile_t *out);
uint8_t tunuino_ini_signature_matches(const tunuino_ini_profile_t *profile, const char *ecu_signature);
#endif
