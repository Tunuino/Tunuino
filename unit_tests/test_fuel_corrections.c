/* Copyright (c) 2026 Tunuino. Independent percentage/timing vectors. */
#include "tunuino/fuel_corrections.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put(uint8_t *p,unsigned value){p[0]=(uint8_t)(value>>8);p[1]=(uint8_t)value;}
static void fixture(uint8_t p[25]){
    memset(p,0,25);p[0]=1;put(p+1,400);put(p+3,15000);put(p+5,11000);put(p+7,1000);
    put(p+9,(uint16_t)(int16_t)-2000);put(p+11,16000);put(p+13,2000);put(p+15,12000);
    put(p+17,8000);put(p+19,10000);put(p+21,1000);p[23]=1;
}
int main(void){
    tunuino_fuel_corrections c;tunuino_fuel_correction_state s={0};uint8_t p[25]={0};uint32_t gain=0;int16_t advance=2300;
    assert(tunuino_fuel_corrections_decode(p,&c));
    assert(tunuino_fuel_corrections_apply(&c,&s,0,0,false,0,&gain,&advance)&&gain==10000&&advance==2300);
    fixture(p);assert(tunuino_fuel_corrections_decode(p,&c));
    assert(tunuino_fuel_corrections_apply(&c,&s,0,200000,true,-20000,&gain,&advance)&&gain==15000&&advance==1000&&!s.running);
    advance=2200;
    assert(tunuino_fuel_corrections_apply(&c,&s,100,1000000,true,20000,&gain,&advance)&&gain==13200&&advance==2200);
    assert(tunuino_fuel_corrections_apply(&c,&s,500100,1000000,true,20000,&gain,&advance)&&gain==12600);
    assert(tunuino_fuel_corrections_apply(&c,&s,1000100,1000000,true,20000,&gain,&advance)&&gain==12000&&!s.afterstart_active);
    assert(tunuino_fuel_corrections_apply(&c,&s,1000200,1000000,true,0,&gain,&advance)&&gain==14000);
    assert(tunuino_fuel_corrections_apply(&c,&s,1000300,1000000,true,120000,&gain,&advance)&&gain==10000);
    assert(tunuino_fuel_corrections_apply(&c,&s,1000400,1000000,true,-40000,&gain,&advance)&&gain==16000);
    assert(!tunuino_fuel_corrections_apply(&c,&s,1000500,1000000,false,20000,&gain,&advance));
    assert(!tunuino_fuel_corrections_apply(&c,&s,1000500,1000000,true,151000,&gain,&advance));
    memset(&s,0,sizeof(s));uint32_t start=UINT32_MAX-400000u;
    assert(tunuino_fuel_corrections_apply(&c,&s,start,400000,true,20000,&gain,&advance)&&gain==13200);
    assert(tunuino_fuel_corrections_apply(&c,&s,start+500000u,400000,true,20000,&gain,&advance)&&gain==12600);
    assert(tunuino_fuel_corrections_apply(&c,&s,start+1000000u,400000,true,20000,&gain,&advance)&&gain==12000);
    assert(tunuino_fuel_corrections_apply(&c,&s,start+1u,400000,true,20000,&gain,&advance)&&gain==12000); /* no re-trigger after clock wrap */
    assert(tunuino_fuel_corrections_apply(&c,&s,10,399999,true,20000,&gain,&advance)&&gain==15000);
    assert(tunuino_fuel_corrections_apply(&c,&s,20,400000,true,20000,&gain,&advance)&&gain==13200); /* new start */
    p[0]=0;assert(tunuino_fuel_corrections_decode(p,&c)&&!c.enabled);
    assert(tunuino_fuel_corrections_apply(&c,&s,30,200000,false,20000,&gain,&advance)&&gain==10000);
    fixture(p);p[24]=1;assert(!tunuino_fuel_corrections_decode(p,&c));
    fixture(p);put(p+7,0);assert(!tunuino_fuel_corrections_decode(p,&c));
    fixture(p);put(p+5,10000);put(p+7,0);assert(tunuino_fuel_corrections_decode(p,&c));
    fixture(p);put(p+13,(uint16_t)(int16_t)-2000);assert(!tunuino_fuel_corrections_decode(p,&c));
    fixture(p);put(p+11,50001);assert(!tunuino_fuel_corrections_decode(p,&c));
    puts("Fuel corrections: exact cranking threshold, fixed advance, coolant interpolation, ASE decay/restart/wrap, retained disabled values, invalid sensors and bounds passed.");
    return 0;
}
