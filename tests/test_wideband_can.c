// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/platform/stm32f429/wideband_can.h"
#include <assert.h>
int main(void) {
    tunuino_can1_frame_t frame = {0};
    tunuino_wideband_sample_t sample = {0};
    frame.identifier = TUNUINO_WIDEBAND_CAN_ID;
    frame.length = 4u;
    frame.data[0] = 0xe8u;
    frame.data[1] = 0x03u;
    frame.data[3] = 42u;
    assert(tunuino_wideband_decode(&frame, &sample));
    assert(sample.valid && sample.lambda_milli == 1000u && sample.counter == 42u);
    frame.data[2] = 1u;
    assert(!tunuino_wideband_decode(&frame, &sample) && !sample.valid);
    frame.data[2] = 0u;
    frame.length = 3u;
    assert(!tunuino_wideband_decode(&frame, &sample));
    frame.length = 4u;
    frame.identifier++;
    assert(!tunuino_wideband_decode(&frame, &sample));
    frame.identifier = TUNUINO_WIDEBAND_CAN_ID;
    frame.extended = 1u;
    assert(!tunuino_wideband_decode(&frame, &sample));
    frame.extended = 0u;
    frame.data[0] = 0u;
    frame.data[1] = 0u;
    assert(!tunuino_wideband_decode(&frame, &sample));
    assert(!tunuino_wideband_decode(0, &sample));
    assert(!tunuino_wideband_decode(&frame, 0));
    tunuino_wideband_monitor_t monitor;
    tunuino_wideband_monitor_init(&monitor);
    assert(!tunuino_wideband_monitor_fresh(&monitor, 0u, 100u));
    frame.data[0] = 0xe8u;
    frame.data[1] = 0x03u;
    assert(tunuino_wideband_monitor_receive(&monitor, &frame, 1000u));
    assert(tunuino_wideband_monitor_fresh(&monitor, 1100u, 100u));
    assert(!tunuino_wideband_monitor_fresh(&monitor, 1101u, 100u));
    assert(!tunuino_wideband_monitor_fresh(&monitor, 1000u, 0u));
    frame.data[2] = 1u;
    assert(!tunuino_wideband_monitor_receive(&monitor, &frame, 1200u));
    assert(!tunuino_wideband_monitor_fresh(&monitor, 1200u, 100u));
    frame.data[2] = 0u;
    assert(tunuino_wideband_monitor_receive(&monitor, &frame, 0xfffffff0u));
    assert(tunuino_wideband_monitor_fresh(&monitor, 0x10u, 40u));
    assert(!tunuino_wideband_monitor_fresh(&monitor, 0x30u, 40u));
    assert(!tunuino_wideband_monitor_fresh(0, 0u, 100u));
    assert(!tunuino_wideband_monitor_receive(0, &frame, 0u));
    return 0;
}
