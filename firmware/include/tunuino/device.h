/* Copyright (c) 2026 Tunuino. */
#ifndef TUNUINO_DEVICE_H
#define TUNUINO_DEVICE_H
#include <stdbool.h>
#include "tunuino/dispatch.h"
#include "tunuino/analog.h"
#include "tunuino/engine_math.h"

#define TUNUINO_IO_SIGNATURE "Tunuino Proteus v0.7 F427 IO v1"
#define TUNUINO_IO_VERSION "Tunuino 0.1.0 Proteus v0.7 IO bench"
/* Serial protocol discovery version. 001 supports the CRC-wrapped envelope.
 * Independent of the application firmware version and INI revision. */
#define TUNUINO_IO_TS_SERIAL_VERSION "001"
#define TUNUINO_IO_CAPABILITIES UINT32_C(15)
#define TUNUINO_IO_OUTPUT_SIZE 100u
/* Native read-only TUNUINO:E response: status, format, valid, reasons,
 * VE centi-percent, ignition centi-degrees (signed), RPM milli, MAP milli,
 * configuration generation. Never permits physical actuation. */
#define TUNUINO_ENGINE_PREVIEW_SIZE 20u
/* Read-only TUNUINO:T: status/version/valid/reasons plus dry-run timing,
 * relative air-load index, signed spark-to-reference interval, generation. */
#define TUNUINO_ENGINE_TIMING_SIZE 32u
#define TUNUINO_ENGINE_NO_RPM 0x01u
#define TUNUINO_ENGINE_NO_MAP 0x02u
#define TUNUINO_ENGINE_TRIGGER_DISABLED 0x04u
#define TUNUINO_ENGINE_NO_SYNC 0x08u
#define TUNUINO_ENGINE_BAD_ADC 0x10u
#define TUNUINO_ENGINE_RPM_LIMIT 0x20u
#define TUNUINO_ENGINE_OUT_OF_TABLE 0x40u
#define TUNUINO_ENGINE_UNCALIBRATED 0x80u
#define TUNUINO_CONFIG_SCHEMA UINT32_C(1)
#define TUNUINO_CONFIG_TRIGGER UINT32_C(2)
#define TUNUINO_CONFIG_MAP UINT32_C(4)
#define TUNUINO_CONFIG_LAMBDA UINT32_C(8)
#define TUNUINO_CONFIG_COOLANT UINT32_C(16)
#define TUNUINO_CONFIG_ADC UINT32_C(32)
#define TUNUINO_CONFIG_VE UINT32_C(64)
#define TUNUINO_CONFIG_IGNITION UINT32_C(128)
#define TUNUINO_IO_CONFIG_SIZE 256u
#define TUNUINO_IO_MAP_SIZE 160u
#define TUNUINO_IO_SETTINGS_SIZE 576u
#define TUNUINO_IO_CHANNELS 16u

typedef bool (*tunuino_device_save)(void *user, const uint8_t *bytes, size_t length);
typedef uint64_t (*tunuino_device_clock)(void *user);
typedef struct tunuino_device {
    uint8_t active[TUNUINO_IO_SETTINGS_SIZE], staged[TUNUINO_IO_SETTINGS_SIZE];
    tunuino_map_snapshot ve, ignition;
    tunuino_map scratch;
    tunuino_live_sample live;
    uint16_t raw[TUNUINO_IO_CHANNELS], tooth;
    uint32_t generation, trigger_errors;
    bool adc_valid, trigger_valid, dirty, persisted;
    tunuino_device_save save;
    tunuino_device_clock clock;
    void *user;
} tunuino_device;

void tunuino_device_init(tunuino_device *device, tunuino_device_save save,
    tunuino_device_clock clock, void *user);
bool tunuino_device_restore(tunuino_device *device, const uint8_t *bytes, size_t length);
bool tunuino_device_validate(const uint8_t *bytes, size_t length);
/* Reports every invalid configuration group; reads only, never alters state. */
uint32_t tunuino_device_validation_errors(const uint8_t *bytes, size_t length);
void tunuino_device_sample(tunuino_device *device, uint64_t time_ms,
    const uint16_t raw[TUNUINO_IO_CHANNELS], bool adc_valid,
    uint32_t rpm_milli, bool rpm_valid, uint16_t tooth, uint32_t errors);
tunuino_dispatch_status tunuino_device_reply(tunuino_device *device,
    const tunuino_identity *identity, const uint8_t *request, size_t length,
    uint8_t *response, size_t capacity, size_t *written);
uint16_t tunuino_device_u16(const uint8_t *bytes);
#endif
