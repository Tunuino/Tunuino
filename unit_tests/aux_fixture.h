/* Copyright (c) 2026 Tunuino. Shared wire fixture, no physical calibration. */
#ifndef TUNUINO_TEST_AUX_FIXTURE_H
#define TUNUINO_TEST_AUX_FIXTURE_H
#include <stdint.h>
#include <string.h>
static void aux_fixture(uint8_t p[256]){
    memset(p,0,256);p[0]=1;
    const unsigned pairs[][2]={{6,400},{8,3600},{10,100},{12,25},{14,0},{16,8000},{18,3500},{20,200},{22,2200},{24,500},{26,100},
        {28,63536},{30,2000},{32,8000},{34,1200},{36,1000},{38,850},{40,3500},{42,2500},{44,1800},{46,400},
        {48,0},{50,5000},{52,0},{54,15000},{56,20000},{58,2500},{60,5000},{62,200},{64,50},{66,500}};
    for(unsigned i=0;i<sizeof(pairs)/sizeof(pairs[0]);++i){p[pairs[i][0]]=(uint8_t)(pairs[i][1]>>8);p[pairs[i][0]+1]=(uint8_t)pairs[i][1];}
}
#endif
