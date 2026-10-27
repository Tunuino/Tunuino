/* Copyright (c) 2026 Tunuino. Physical units and coherent synthetic samples. */
#include "tunuino/engine_features.h"
#include "feature_fixture.h"
#include <assert.h>
#include <stdio.h>
static void reset(tunuino_feature_state *s){memset(s,0,sizeof(*s));}
int main(void){
    uint8_t p[256]={0};tunuino_feature_config c;tunuino_feature_state s={0};tunuino_feature_inputs in={0};
    assert(tunuino_features_decode(p,&c)&&c.flags==0);feature_fixture(p);assert(tunuino_features_decode(p,&c));
    p[70]=8;assert(!tunuino_features_decode(p,&c));p[70]=0;p[255]=1;assert(!tunuino_features_decode(p,&c));p[255]=0;
    p[71]=8;assert(!tunuino_features_decode(p,&c));p[71]=0;assert(tunuino_features_decode(p,&c));
    c.iat_channel=2;c.voltage_channel=4;c.flex_channel=5;c.launch_channel=6;
    uint16_t raw[16]={0};raw[13]=2000;raw[3]=100;raw[4]=2000;raw[5]=2500;
    tunuino_features_sample(&c,true,raw,true,200,true,990000,&in);
    assert(in.iat_valid&&in.iat==2000&&in.voltage_valid&&in.voltage==500&&in.flex_valid&&in.ethanol==5000&&in.launch_valid&&in.launch);
    raw[13]=0;raw[3]=4095;raw[4]=0;raw[5]=0;tunuino_features_sample(&c,true,raw,false,0,false,0,&in);
    assert(!in.iat_valid&&!in.voltage_valid&&!in.flex_valid&&!in.launch_valid);
    in=(tunuino_feature_inputs){.tps_valid=true,.iat_valid=true,.voltage_valid=true,.launch_valid=true,.flex_valid=true,.lambda_valid=true,.iat=2000,.voltage=1000,.lambda=990000};
    assert(tunuino_features_update(&c,&s,&in,0,3000000,100000,true,80000,400,500,2000)&&s.multiplier==10000&&s.dead_us==500&&s.dwell_us==2000);
    c.flags=TUNUINO_FEATURE_IAT;reset(&s);in.iat=6000;
    assert(tunuino_features_update(&c,&s,&in,0,3000000,100000,true,80000,400,500,2000)&&s.multiplier==8799&&s.advance_delta==-100);
    in.iat_valid=false;assert(!tunuino_features_update(&c,&s,&in,1000,3000000,100000,true,80000,400,500,2000));in.iat_valid=true;
    c.flags=TUNUINO_FEATURE_VOLTAGE;reset(&s);
    assert(tunuino_features_update(&c,&s,&in,0,3000000,100000,true,80000,400,500,2000)&&s.dead_us==1250&&s.dwell_us==3500);
    c.flags=TUNUINO_FEATURE_TARGET;reset(&s);
    assert(tunuino_features_update(&c,&s,&in,0,3000000,100000,true,80000,400,500,2000)&&s.target==900&&s.multiplier==11111);
    c.flags=TUNUINO_FEATURE_FLEX;c.flex_advance=400;in.ethanol=5000;reset(&s);
    assert(tunuino_features_update(&c,&s,&in,0,3000000,100000,true,80000,400,500,2000)&&s.multiplier==13000&&s.advance_delta==200);
    c.flags=TUNUINO_FEATURE_TARGET|TUNUINO_FEATURE_LAMBDA;c.lambda_delay=0;reset(&s);
    assert(tunuino_features_update(&c,&s,&in,0,3000000,100000,true,80000,400,500,2000)&&!s.closed_loop);
    assert(tunuino_features_update(&c,&s,&in,20000,3000000,100000,true,80000,400,500,2000)&&s.closed_loop&&s.lambda_trim==450&&s.multiplier==11610);
    for(uint32_t n=40000;n<20000000;n+=20000){in.lambda=2000000;assert(tunuino_features_update(&c,&s,&in,n,3000000,100000,true,80000,400,500,2000)&&s.lambda_trim<=1000);}
    assert(s.integral<10000000); /* saturated proportional response does not wind up */
    in.lambda_valid=false;assert(tunuino_features_update(&c,&s,&in,20000000,3000000,100000,true,80000,400,500,2000)&&s.lambda_trim==0&&!s.closed_loop);
    c.flags=TUNUINO_FEATURE_TARGET|TUNUINO_FEATURE_LAMBDA|TUNUINO_FEATURE_ACCEL;c.transient_delay=100;reset(&s);in.lambda_valid=true;in.lambda=990000;in.tps=0;
    assert(tunuino_features_update(&c,&s,&in,0,3000000,100000,true,80000,400,500,2000));
    assert(tunuino_features_update(&c,&s,&in,20000,3000000,100000,true,80000,400,500,2000)&&s.closed_loop);
    in.tps=200;
    for(uint32_t n=40000;n<=340000;n+=20000){assert(tunuino_features_update(&c,&s,&in,n,3000000,100000,true,80000,400,500,2000));if(n<320000)assert(!s.closed_loop&&s.lambda_trim==0);}
    assert(s.closed_loop&&!s.inhibit);
    c.flags=TUNUINO_FEATURE_TARGET|TUNUINO_FEATURE_LAMBDA;reset(&s);uint32_t wrapping=UINT32_MAX-10000u;
    for(unsigned i=0;i<44000;++i,wrapping+=100000u){assert(tunuino_features_update(&c,&s,&in,wrapping,3000000,100000,true,80000,400,500,2000));if(i>1)assert(s.closed_loop&&s.run_delay_done);}
    c.flags=TUNUINO_FEATURE_ACCEL;reset(&s);in.tps=0;
    assert(tunuino_features_update(&c,&s,&in,UINT32_MAX-10000u,3000000,100000,true,80000,400,500,2000));
    in.tps=200;assert(tunuino_features_update(&c,&s,&in,9999,3000000,100000,true,80000,400,500,2000)&&s.extra_us==1000);
    assert(tunuino_features_update(&c,&s,&in,29999,3000000,100000,true,80000,400,500,2000)&&s.extra_us==900);
    for(uint32_t n=49999;n<=209999;n+=20000)assert(tunuino_features_update(&c,&s,&in,n,3000000,100000,true,80000,400,500,2000));
    assert(!s.accel_active&&s.extra_us==0);
    c.flags=TUNUINO_FEATURE_DFCO;reset(&s);in.tps=0;
    for(uint32_t n=0;n<=500000;n+=20000){assert(tunuino_features_update(&c,&s,&in,n,3000000,100000,true,80000,400,500,2000));if(n<500000)assert(!s.fuel_cut);}
    assert(s.dfco&&s.fuel_cut);assert(tunuino_features_update(&c,&s,&in,520000,2100000,100000,true,80000,400,500,2000)&&s.fuel_cut);
    assert(tunuino_features_update(&c,&s,&in,540000,1800000,100000,true,80000,400,500,2000)&&!s.fuel_cut);
    c.flags=TUNUINO_FEATURE_LIMIT|TUNUINO_FEATURE_LAUNCH;reset(&s);
    assert(tunuino_features_update(&c,&s,&in,0,6500000,100000,true,80000,400,500,2000)&&s.fuel_cut);
    assert(tunuino_features_update(&c,&s,&in,20000,6300000,100000,true,80000,400,500,2000)&&s.fuel_cut);
    assert(tunuino_features_update(&c,&s,&in,40000,6000000,100000,true,80000,400,500,2000)&&!s.fuel_cut);
    in.launch=true;assert(tunuino_features_update(&c,&s,&in,60000,3500000,100000,true,80000,400,500,2000)&&s.launch_limited&&s.fuel_cut);
    in.launch=false;assert(tunuino_features_update(&c,&s,&in,80000,3500000,100000,true,80000,400,500,2000)&&!s.launch_limited&&!s.fuel_cut);
    c.flags=TUNUINO_FEATURE_PROTECTION;reset(&s);in.voltage=1200;
    assert(!tunuino_features_update(&c,&s,&in,0,3000000,100000,true,110000,400,500,2000)&&s.protection);
    assert(tunuino_features_update(&c,&s,&in,1,3000000,100000,true,80000,400,500,2000));
    in.voltage=899;assert(!tunuino_features_update(&c,&s,&in,2,3000000,100000,true,80000,400,500,2000)&&s.protection);
    puts("Engine features: sensor calibration, ideal-gas units, voltage curves, lambda target/PI/gates/anti-windup, ethanol blend, TPS enrichment, DFCO delay/hysteresis, RPM/launch cuts, protections and counter wrap passed.");return 0;
}
