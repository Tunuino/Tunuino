/* Copyright (c) 2026 Tunuino. Independent idle/boost and throttle vectors. */
#include "tunuino/aux_control.h"
#include "aux_fixture.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    uint8_t p[256]={0};tunuino_aux_control a={0};assert(tunuino_aux_decode(p,0,false,&a.config));
    aux_fixture(p);assert(tunuino_aux_decode(p,0,false,&a.config));
    p[1]=2;p[2]=8;p[3]=2;p[4]=9;p[5]=3;
    assert(!tunuino_aux_decode(p,0,false,&a.config));assert(!tunuino_aux_decode(p,1u<<7,true,&a.config));
    p[4]=8;assert(!tunuino_aux_decode(p,0,true,&a.config));p[4]=9;
    assert(tunuino_aux_decode(p,1,true,&a.config));uint16_t raw[16]={0},tps=0;
    raw[2]=2000;assert(tunuino_aux_tps(&a.config,true,raw,&tps)&&tps==5000);
    raw[2]=399;assert(tunuino_aux_tps(&a.config,true,raw,&tps)&&tps==0);
    raw[2]=0;assert(!tunuino_aux_tps(&a.config,true,raw,&tps));
    a.config.tps_closed=3600;a.config.tps_open=400;raw[2]=2000;assert(tunuino_aux_tps(&a.config,true,raw,&tps)&&tps==5000);
    assert(tunuino_aux_update(&a,0,true,false,200000,true,0,true,20000,true,100000)&&a.idle_duty==3500&&a.boost_duty==0);
    assert(tunuino_aux_update(&a,1000,true,true,800000,true,0,true,20000,true,100000));assert(a.idle_target==1000&&a.idle_duty==3500); /* base25% + P10% */
    assert(tunuino_aux_update(&a,21000,true,true,800000,true,0,true,20000,true,100000)&&a.idle_duty==3504); /* +0.04% integral */
    assert(tunuino_aux_update(&a,22000,true,true,800000,true,300,true,20000,true,100000)&&a.idle_duty==0&&!a.idle.active);
    assert(tunuino_aux_update(&a,23000,true,true,800000,true,0,false,20000,true,100000)&&a.idle_duty==0);
    assert(tunuino_aux_update(&a,24000,true,true,2500000,true,6000,true,20000,true,100000)&&a.boost_duty==1000); /* P = 2% / 10kPa * 50kPa */
    assert(tunuino_aux_update(&a,44000,true,true,2500000,true,6000,true,20000,true,100000)&&a.boost_duty==1005);
    assert(tunuino_aux_update(&a,45000,true,true,2500000,false,0,true,20000,true,100000)&&a.boost_duty==0&&!a.boost.active);
    assert(!tunuino_aux_update(&a,46000,true,true,2500000,true,6000,true,20000,true,200000)&&a.boost_duty==0&&a.idle_duty==0);
    assert(tunuino_aux_update(&a,47000,false,false,0,false,0,false,0,false,0)&&!a.boost.active&&!a.idle.active);
    assert(tunuino_aux_decode(p,1,true,&a.config));a.config.idle_p=5000;a.config.idle_i=5000;
    for(uint32_t now=0;now<2000000;now+=20000)assert(tunuino_aux_update(&a,now,true,true,500000,true,0,true,20000,true,100000)&&a.idle_duty<=8000);
    assert(a.idle.integral==0); /* saturation does not accumulate windup */
    assert(tunuino_aux_update(&a,2000000,true,true,1100000,true,0,true,20000,true,100000)&&a.idle_duty==0);
    tunuino_aux_reset(&a);a.config.idle_p=500;a.config.idle_i=100;
    assert(tunuino_aux_update(&a,UINT32_MAX-10000u,true,true,800000,true,0,true,20000,true,100000)&&a.idle_duty==3500);
    assert(tunuino_aux_update(&a,9999u,true,true,800000,true,0,true,20000,true,100000)&&a.idle_duty==3504);
    a.config.idle_mode=1;assert(tunuino_aux_update(&a,30000,true,true,900000,true,0,true,50000,true,100000)&&a.idle_target==925&&a.idle_duty==2150);
    p[68]=1;assert(!tunuino_aux_decode(p,0,true,&a.config));
    puts("Aux control: TPS both directions, output ownership, interpolation, cranking, PI timing, anti-windup, gates, overboost cut and counter wrap passed.");return 0;
}
