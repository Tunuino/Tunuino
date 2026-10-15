// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_protocol.h"
#include <stdio.h>
static void emit(const char *name, const uint8_t *data, size_t size) {
    printf("%s ", name);
    for (size_t i = 0; i < size; ++i) printf("%02x", (unsigned)data[i]);
    putchar('\n');
}
int main(void) {
    uint8_t frame[72];
    const uint8_t hello[] = {0x54u, 0x4eu, 1u};
    size_t size = tunuino_console_encode(TUNUINO_CONSOLE_TYPE_HELLO, hello, sizeof(hello), frame, sizeof(frame));
    if (size != 11u) return 1;
    emit("hello", frame, size);
    const tunuino_console_telemetry_t sample = {1500u, 1013u, 325u, 1250u, 650u, 1470u, 1380u};
    size = tunuino_console_encode_telemetry(&sample, frame, sizeof(frame));
    if (size != 22u) return 1;
    emit("telemetry", frame, size);
    return 0;
}
