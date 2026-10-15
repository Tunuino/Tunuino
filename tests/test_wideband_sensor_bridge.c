// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/platform/stm32f429/wideband_sensor_bridge.h"
#include <assert.h>
int main(void) {
    tunuino_wideband_sensor_bridge_t bridge;
    tunuino_can1_frame_t frame = {0};
    uint16_t lambda = 123u;
    tunuino_wideband_sensor_bridge_init(&bridge);
    assert(!tunuino_wideband_sensor_bridge_read(&bridge, 0u, 100u, &lambda));
    assert(lambda == 0u);
    frame.identifier = TUNUINO_WIDEBAND_CAN_ID;
    frame.length = 4u;
    frame.data[0] = 0xe8u;
    frame.data[1] = 0x03u;
    frame.data[3] = 1u;
    assert(tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1000u));
    assert(tunuino_wideband_sensor_bridge_read(&bridge, 1100u, 100u, &lambda));
    assert(lambda == 1000u);
    assert(!tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1100u));
    assert(!tunuino_wideband_sensor_bridge_read(&bridge, 1101u, 100u, &lambda));
    assert(lambda == 0u);
    frame.data[3] = 2u;
    assert(tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1200u));
    assert(tunuino_wideband_sensor_bridge_read(&bridge, 1201u, 100u, &lambda));
    assert(lambda == 1000u);
    frame.data[2] = 1u;
    frame.data[3] = 3u;
    assert(!tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1250u));
    assert(!tunuino_wideband_sensor_bridge_read(&bridge, 1250u, 100u, &lambda));
    assert(lambda == 0u);
    frame.data[2] = 0u;
    // Recovery is permitted even when the sender restarted its counter.
    assert(tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1260u));
    assert(tunuino_wideband_sensor_bridge_read(&bridge, 1261u, 100u, &lambda));
    frame.data[0] = 0u;
    frame.data[1] = 0u;
    frame.data[3] = 4u; // New counter: invalid sample must revoke the measurement.
    assert(!tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1270u));
    assert(!tunuino_wideband_sensor_bridge_read(&bridge, 1270u, 100u, &lambda));
    frame.data[0] = 0xe8u;
    frame.data[1] = 0x03u;
    assert(tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1280u));
    frame.identifier++;
    assert(!tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1281u));
    assert(tunuino_wideband_sensor_bridge_read(&bridge, 1282u, 100u, &lambda));
    frame.identifier = TUNUINO_WIDEBAND_CAN_ID;
    frame.data[2] = 1u;
    assert(!tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1283u));
    assert(tunuino_wideband_sensor_bridge_read(&bridge, 1284u, 100u, &lambda));
    assert(lambda == 1000u);
    frame.data[3] = 5u;
    assert(!tunuino_wideband_sensor_bridge_receive(&bridge, &frame, 1285u));
    assert(!tunuino_wideband_sensor_bridge_read(&bridge, 1285u, 100u, &lambda));
    assert(!tunuino_wideband_sensor_bridge_read(0, 0u, 100u, &lambda));
    assert(!tunuino_wideband_sensor_bridge_read(&bridge, 0u, 100u, 0));
    assert(!tunuino_wideband_sensor_bridge_receive(0, &frame, 0u));
    return 0;
}
