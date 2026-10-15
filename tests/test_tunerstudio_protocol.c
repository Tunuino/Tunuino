// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/tunerstudio_protocol.h"
#include <assert.h>
#include <string.h>
int main(void) {
    uint8_t out[32] = {0};
    assert(tunuino_ts_reply('S', 0, out, sizeof(out)) == strlen(TUNUINO_TS_SIGNATURE));
    assert(memcmp(out, TUNUINO_TS_SIGNATURE, strlen(TUNUINO_TS_SIGNATURE)) == 0);
    const tunuino_ts_values_t data = {1234u, 1013u, 1250u, 1u, 0u, 1u};
    assert(tunuino_ts_reply('A', &data, out, 8u) == 8u);
    assert(out[0] == 0xd2u && out[1] == 0x04u);
    assert(out[2] == 0xffu && out[3] == 0xffu);
    assert(out[4] == 0xe2u && out[5] == 0x04u);
    assert(out[6] == 0u && out[7] == 0u);
    assert(tunuino_ts_reply('A', &data, out, 7u) == 0u);
    assert(tunuino_ts_reply('W', &data, out, sizeof(out)) == 0u);
    return 0;
}
