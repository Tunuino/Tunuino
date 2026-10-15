// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
// Copyright (c) 2026 Tunuino
#include "../firmware/core/console_tune_request.h"
#include "../firmware/core/console_tx_queue.h"
#include <assert.h>
int main(void) {
    tunuino_tune_maps_t maps = {0};
    for (unsigned t=0;t<2u;t++) {
        tunuino_tune_map_t *m=t?&maps.ignition:&maps.ve;
        for (unsigned i=0;i<TUNUINO_MAP_AXIS_SIZE;i++) {
            m->rpm_axis[i]=(uint16_t)(1000u+500u*i);
            m->load_axis[i]=(uint16_t)(200u+100u*i);
            for (unsigned j=0;j<TUNUINO_MAP_AXIS_SIZE;j++) m->cells[i][j]=t?120:850;
        }
    }
    tunuino_tune_store_t store={0};
    tunuino_tune_wire_t wire={0};
    tunuino_tx_queue_t queue={0};
    tunuino_tx_queue_init(&queue);
    assert(tunuino_tune_store_init(&store,&maps)==TUNUINO_TUNE_OK);
    assert(tunuino_tune_wire_build(&wire,&store));
    uint8_t request[11]={0x54,0x4e,1,5,3,0,0,0,32,0,0};
    uint16_t crc=tunuino_console_crc16(request+2,7);
    request[9]=(uint8_t)crc;request[10]=(uint8_t)(crc>>8);
    assert(tunuino_console_tune_handle_frame(&wire,request,sizeof(request),&queue));
    assert(tunuino_tx_queue_size(&queue)==43u);
    request[9]^=1u;
    assert(!tunuino_console_tune_handle_frame(&wire,request,sizeof(request),&queue));
    assert(tunuino_tx_queue_size(&queue)==43u);
    request[9]^=1u;request[3]=99u;
    assert(!tunuino_console_tune_handle_frame(&wire,request,sizeof(request),&queue));
    return 0;
}
