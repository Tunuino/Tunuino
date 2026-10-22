/* Copyright (c) 2026 Tunuino. Experimental laboratory USB identity only.
 * Development VID/PID follow TinyUSB examples; not a production allocation. */
#include "tusb.h"
#include "board.h"
#include <string.h>

static const tusb_desc_device_t device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON, .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = 64, .idVendor = 0xCAFE, .idProduct = 0x4001,
    .bcdDevice = 0x0001, .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3,
    .bNumConfigurations = 1
};
static const uint8_t configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1,2,0,TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN,0,100),
    TUD_CDC_DESCRIPTOR(0,4,0x81,8,0x02,0x82,64)
};
const uint8_t *tud_descriptor_device_cb(void) { return (const uint8_t *)&device; }
const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    return index == 0 ? configuration : NULL;
}
const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t string[33];
    const char *text;
    unsigned count;
    if (index == 0) { string[0] = 0x0304; string[1] = 0x0409; return string; }
    if (index == 3) {
        uint8_t uid[12]; board_uid(uid);
        static const char hex[] = "0123456789abcdef";
        for (unsigned i = 0; i < 12; ++i) {
            string[1 + i * 2] = (uint16_t)hex[uid[i] >> 4];
            string[2 + i * 2] = (uint16_t)hex[uid[i] & 15];
        }
        count = 24;
    } else {
        switch (index) {
            case 1: text = "Tunuino"; break;
            case 2: text = "Proteus v0.7 USB bench"; break;
            case 4: text = "Tunuino Console"; break;
            default: return NULL;
        }
        count = (unsigned)strlen(text);
        if (count > 32) count = 32;
        for (unsigned i = 0; i < count; ++i) string[i + 1] = (uint16_t)text[i];
    }
    string[0] = (uint16_t)(0x0300u | (2u + 2u * count));
    return string;
}
