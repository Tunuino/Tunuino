/* Copyright (c) 2026 Tunuino. Synthetic clock/GPIO, no physical loads. */
#include "tunuino/engine_control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "feature_fixture.h"
static uint32_t clock_us,mask,rises[28],falls[28],on_time[28],last_width[28];
static void gpio(void *user,unsigned ch,bool high){
    (void)user;assert(ch<28);
    if(high){assert(!(mask&(UINT32_C(1)<<ch)));mask|=UINT32_C(1)<<ch;++rises[ch];on_time[ch]=clock_us;}
    else {if(mask&(UINT32_C(1)<<ch)){++falls[ch];last_width[ch]=clock_us-on_time[ch];}mask&=~(UINT32_C(1)<<ch);}
}
static void p16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void p32(uint8_t *p,uint32_t v){p16(p,v>>16);p16(p+2,v);}
static void profile(uint8_t p[128]){
    memset(p,0,128);p[0]=p[1]=1;p16(p+2,3000);p16(p+4,2000);p16(p+6,500);
    p16(p+8,15000);p16(p+10,1000);p16(p+12,9500);p16(p+14,9000);
    p[22]=14;p[23]=15;p[24]=16;p[25]=1;p[26]=2;
    p32(p+34,18000);
}
int main(void){
    tunuino_outputs o;clock_us=0;tunuino_outputs_init(&o,gpio,NULL);assert(!mask&&!o.armed);
    assert(!tunuino_outputs_pulse(&o,0,0,10,100));
    tunuino_outputs_arm(&o,0,10000);assert(!tunuino_outputs_pulse(&o,16,0,10,5001));
    assert(!tunuino_outputs_pulse(&o,28,0,10,100));
    assert(tunuino_outputs_pulse(&o,16,0,100,2000));
    assert(!tunuino_outputs_pulse(&o,16,0,200,2000));
    clock_us=100;tunuino_outputs_poll(&o,clock_us);assert(mask==(UINT32_C(1)<<16));
    clock_us=2100;tunuino_outputs_poll(&o,clock_us);assert(!mask&&last_width[16]==2000);
    assert(tunuino_outputs_level(&o,0,true));clock_us=10000;tunuino_outputs_poll(&o,clock_us);
    assert(!mask&&o.fault==TUNUINO_OUTPUT_WATCHDOG&&!o.armed);
    tunuino_outputs_arm(&o,10000,10000);assert(tunuino_outputs_pulse(&o,0,10000,10100,100));
    clock_us=10201;tunuino_outputs_poll(&o,clock_us);assert(!mask&&o.fault==TUNUINO_OUTPUT_LATE);
    tunuino_outputs_arm(&o,UINT32_MAX-50,10000);
    assert(tunuino_outputs_pulse(&o,0,UINT32_MAX-50,20,500));
    clock_us=20;tunuino_outputs_poll(&o,20);assert(mask==1);
    clock_us=520;tunuino_outputs_poll(&o,520);assert(!mask);
    tunuino_outputs_stop(&o,0);
    tunuino_outputs_arm(&o,0,10000);assert(tunuino_outputs_pulse(&o,0,0,100,2000)&&tunuino_outputs_pulse(&o,16,0,100,2000));
    clock_us=100;tunuino_outputs_poll(&o,clock_us);assert(mask==((1u<<16)|1u));
    tunuino_outputs_cancel_injector(&o,0);assert(mask==(1u<<16)&&!o.pulse[0].state&&o.pulse[16].state==2);
    tunuino_outputs_cancel_injector(&o,16);assert(mask==(1u<<16)); /* fuel cut cannot truncate ignition dwell */
    clock_us=2100;tunuino_outputs_poll(&o,clock_us);assert(!mask&&last_width[16]==2000);tunuino_outputs_stop(&o,0);
    assert(!tunuino_outputs_aux_test(&o,16,0,500000));
    assert(!tunuino_outputs_aux_test(&o,15,0,800001));
    assert(tunuino_outputs_aux_test(&o,15,UINT32_MAX-500,800000));
    assert(!tunuino_outputs_aux_test(&o,14,0,1000)); /* No overlapping tests. */
    clock_us=499;tunuino_outputs_poll(&o,clock_us);assert(mask==(UINT32_C(1)<<15));
    clock_us=800499;tunuino_outputs_poll(&o,clock_us);assert(!mask&&last_width[15]==800000);
    tunuino_outputs_stop(&o,0);
    uint8_t p[128],bad[128];profile(p);tunuino_engine_profile cfg;
    assert(tunuino_engine_profile_decode(p,60,2,&cfg));
    memcpy(bad,p,128);bad[1]=2;assert(!tunuino_engine_profile_decode(bad,60,2,&cfg));
    memcpy(bad,p,128);bad[22]=1;assert(!tunuino_engine_profile_decode(bad,60,2,&cfg));
    memcpy(bad,p,128);bad[27]=2;assert(!tunuino_engine_profile_decode(bad,60,2,&cfg));
    memcpy(bad,p,128);bad[127]=1;assert(!tunuino_engine_profile_decode(bad,60,2,&cfg));
    tunuino_engine_control e;tunuino_engine_control_init(&e,gpio,NULL);
    assert(!tunuino_engine_control_arm(&e,0,1));
    assert(tunuino_engine_control_configure(&e,p,60,2,1));
    assert(!tunuino_engine_control_arm(&e,0,2));
    assert(tunuino_engine_control_arm(&e,0,1));
    memset(rises,0,sizeof(rises));memset(falls,0,sizeof(falls));
    for(clock_us=0;clock_us<=360000;clock_us+=10){
        tunuino_outputs_poll(&e.outputs,clock_us);
        if(clock_us%250==0){
            unsigned slot=clock_us/1000u,rev=slot/60u,tooth=slot%60u;
            if(tooth>57)tooth=57;
            uint32_t edge=(rev*60u+tooth)*1000u;
            tunuino_position_reading r={1,1,(uint8_t)(rev%2), (uint16_t)tooth,1000.0};
            tunuino_engine_control_update(&e,clock_us,edge,&r,60,1,true,10000,1000,100000,true,92000);
            assert(!e.outputs.fault);
        }
    }
    assert(rises[0]==3&&falls[0]==3&&last_width[0]==3500);
    assert(rises[16]==3&&falls[16]==3&&last_width[16]==2000);
    assert(mask& (UINT32_C(1)<<13));assert(mask&(UINT32_C(1)<<15));
    assert(!(mask&(UINT32_C(1)<<14)));assert(e.last_injection_us==3500);
    tunuino_position_reading lost={0};
    tunuino_engine_control_update(&e,clock_us,clock_us,&lost,60,1,false,0,0,0,false,0);
    assert(!mask&&e.outputs.fault==TUNUINO_OUTPUT_SYNC);
    assert(tunuino_engine_control_arm(&e,clock_us,1));
    tunuino_engine_control_update(&e,clock_us,clock_us,&lost,60,2,false,0,0,0,false,0);
    assert(!e.configured&&!mask&&e.outputs.fault==TUNUINO_OUTPUT_CALIBRATION);
    /* Twelve independent cylinders across six speeds; use physical 60-2
     * tooth timestamps, including both missing slots and cycle rollover. */
    const unsigned speeds[]={400,800,1500,3000,6000,12000};
    for(unsigned speed=0;speed<sizeof(speeds)/sizeof(speeds[0]);++speed){
        unsigned rpm=speeds[speed];profile(p);p[1]=12;
        for(unsigned i=0;i<12;++i){p[32+8*i]=(uint8_t)i;p[33+8*i]=(uint8_t)i;p32(p+34+8*i,i*6000);}
        clock_us=0;tunuino_engine_control_init(&e,gpio,NULL);
        assert(tunuino_engine_control_configure(&e,p,60,2,1));assert(tunuino_engine_control_arm(&e,0,1));
        memset(rises,0,sizeof(rises));memset(falls,0,sizeof(falls));
        for(clock_us=0;clock_us<600000;clock_us+=10){
            tunuino_outputs_poll(&e.outputs,clock_us);
            if(clock_us%250==0){
                unsigned slot=(unsigned)((uint64_t)clock_us*rpm/1000000u),rev=slot/60,tooth=slot%60;
                if(tooth>57)tooth=57;
                uint32_t edge=(uint32_t)(((uint64_t)(rev*60+tooth)*1000000u+rpm/2)/rpm);
                tunuino_position_reading pos={1,1,(uint8_t)(rev%2),(uint16_t)tooth,(double)rpm};
                tunuino_engine_control_update(&e,clock_us,edge,&pos,60,1,true,10000,1000,100000,true,92000);
                if(e.outputs.fault)fprintf(stderr,"rpm=%u now=%u edge=%u tooth=%u fault=%u\n",rpm,clock_us,edge,tooth,e.outputs.fault);
                assert(!e.outputs.fault);
            }
        }
        for(unsigned i=0;i<12;++i){assert(rises[i]>=1&&rises[16+i]>=1);}
        tunuino_engine_control_stop(&e,0);assert(!mask);
    }
    profile(p);tunuino_engine_control_init(&e,gpio,NULL);
    const uint32_t origin=UINT32_MAX-60000;
    assert(tunuino_engine_control_configure(&e,p,60,2,1));assert(tunuino_engine_control_arm(&e,origin,1));
    for(uint32_t elapsed=0;elapsed<180000;elapsed+=10){
        clock_us=origin+elapsed;tunuino_outputs_poll(&e.outputs,clock_us);
        if(elapsed%250==0){
            unsigned slot=elapsed/1000,rev=slot/60,tooth=slot%60;if(tooth>57)tooth=57;
            tunuino_position_reading pos={1,1,(uint8_t)(rev%2),(uint16_t)tooth,1000.0};
            tunuino_engine_control_update(&e,clock_us,origin+(rev*60+tooth)*1000,&pos,60,1,true,10000,1000,100000,true,92000);
            assert(!e.outputs.fault);
        }
    }
    tunuino_engine_control_stop(&e,0);assert(!mask);
    /* The actual GPIO scheduler uses corrected fuel, adds dead time once,
     * and still refuses a corrected pulse above the configured limit. */
    uint8_t correction[25]={1};p16(correction+1,700);p16(correction+3,15000);p16(correction+5,11000);p16(correction+7,1000);
    p16(correction+9,(uint16_t)(int16_t)-2000);p16(correction+11,16000);p16(correction+13,2000);p16(correction+15,12000);
    p16(correction+17,8000);p16(correction+19,10000);p16(correction+21,1000);correction[23]=1;
    profile(p);clock_us=0;tunuino_engine_control_init(&e,gpio,NULL);
    assert(tunuino_engine_control_configure(&e,p,60,2,1));
    assert(tunuino_fuel_corrections_decode(correction,&e.fueling));assert(tunuino_engine_control_arm(&e,0,1));
    memset(rises,0,sizeof(rises));memset(falls,0,sizeof(falls));
    for(clock_us=0;clock_us<=600000;clock_us+=10){
        tunuino_outputs_poll(&e.outputs,clock_us);
        if(clock_us%250==0){
            unsigned slot=(unsigned)((uint64_t)clock_us*600u/1000000u),rev=slot/60,tooth=slot%60;if(tooth>57)tooth=57;
            uint32_t edge=(uint32_t)(((uint64_t)(rev*60+tooth)*1000000u+300u)/600u);
            tunuino_position_reading pos={1,1,(uint8_t)(rev%2),(uint16_t)tooth,600.0};
            tunuino_engine_control_update(&e,clock_us,edge,&pos,60,1,true,10000,2000,100000,true,20000);
            assert(!e.outputs.fault&&e.last_injection_us==5000);
        }
    }
    assert(rises[0]>=2&&last_width[0]==5000&&last_width[16]==2000);
    tunuino_engine_control_stop(&e,0);assert(!mask);
    p16(p+8,4500);assert(tunuino_engine_control_configure(&e,p,60,2,1));
    assert(tunuino_engine_control_arm(&e,clock_us,1));
    tunuino_position_reading start_position={1,1,0,0,600.0};
    tunuino_engine_control_update(&e,clock_us,clock_us,&start_position,60,1,true,10000,2000,100000,true,20000);
    assert(e.outputs.fault==TUNUINO_OUTPUT_CALIBRATION&&!mask); /* rich pulse is cut, never silently clipped */
    uint8_t features[256]={0};feature_fixture(features);tunuino_feature_config fc;assert(tunuino_features_decode(features,&fc));
    for(unsigned scenario=0;scenario<2;++scenario){
        profile(p);clock_us=0;tunuino_engine_control_init(&e,gpio,NULL);
        assert(tunuino_engine_control_configure(&e,p,60,2,1));
        e.features=fc;e.features.flags=scenario?TUNUINO_FEATURE_LIMIT:TUNUINO_FEATURE_IAT|TUNUINO_FEATURE_VOLTAGE|TUNUINO_FEATURE_TARGET|TUNUINO_FEATURE_FLEX|TUNUINO_FEATURE_TRIMS;
        e.features.fuel_trim[0]=1000;e.features.spark_trim[0]=200;e.features.limit_cut=900;e.features.limit_resume=800;
        e.feature_inputs=(tunuino_feature_inputs){.iat_valid=true,.voltage_valid=true,.flex_valid=true,.iat=2000,.voltage=1000,.ethanol=5000};
        assert(tunuino_engine_control_arm(&e,0,1));memset(rises,0,sizeof(rises));
        for(clock_us=0;clock_us<=360000;clock_us+=10){
            tunuino_outputs_poll(&e.outputs,clock_us);
            if(clock_us%250==0){unsigned slot=clock_us/1000u,rev=slot/60u,tooth=slot%60u;if(tooth>57)tooth=57;
                tunuino_position_reading r={1,1,(uint8_t)(rev%2),(uint16_t)tooth,1000};
                tunuino_engine_control_update(&e,clock_us,(rev*60u+tooth)*1000u,&r,60,1,true,10000,1000,100000,true,90000);
                assert(e.outputs.armed&&!e.outputs.fault);
            }
        }
        if(scenario)assert(rises[0]==0&&rises[16]>=2&&e.last_injection_us==0&&e.feature_state.fuel_cut);
        else assert(rises[0]>=2&&e.outputs.pulse[0].end-e.outputs.pulse[0].start==6016
            &&last_width[0]>=6006&&last_width[0]<=6026&&last_width[16]==3500&&e.last_injection_us==5583); /* 10us host polling grid */
        tunuino_engine_control_stop(&e,0);assert(!mask);
    }
    assert(tunuino_engine_control_arm(&e,clock_us,1));e.features.flags=TUNUINO_FEATURE_PROTECTION;e.feature_inputs.voltage=899;
    tunuino_position_reading protected_position={1,1,0,0,1000};
    tunuino_engine_control_update(&e,clock_us,clock_us,&protected_position,60,1,true,10000,1000,100000,true,90000);
    assert(!e.outputs.armed&&!mask&&e.outputs.fault==TUNUINO_OUTPUT_PROTECTION);
    /* Real scheduler code: crank-only modes, twice per 720 degrees, independent
     * of absent/changing cam phase. Separate outputs remain mandatory. */
    for(unsigned operation=1;operation<=2;++operation){
        profile(p);p[0]=2;p[28]=(uint8_t)operation;p[1]=4;
        for(unsigned i=0;i<4;++i){p[32+8*i]=(uint8_t)i;p[33+8*i]=(uint8_t)i;p32(p+34+8*i,i*18000);}
        clock_us=0;tunuino_engine_control_init(&e,gpio,NULL);
        assert(tunuino_engine_control_configure(&e,p,60,2,1));
        assert(e.profile.operation==operation&&tunuino_engine_control_arm(&e,0,1));
        memset(rises,0,sizeof(rises));memset(falls,0,sizeof(falls));
        for(clock_us=0;clock_us<360000;clock_us+=10){
            tunuino_outputs_poll(&e.outputs,clock_us);
            if(clock_us%250==0){
                unsigned rev=clock_us/60000,tooth=(clock_us/1000)%60;if(tooth>57)tooth=57;
                tunuino_position_reading crank_only={1,0,(uint8_t)(rev%2),(uint16_t)tooth,1000};
                tunuino_engine_control_update(&e,clock_us,(rev*60+tooth)*1000,&crank_only,60,1,true,10000,1000,100000,true,92000);
                assert(!e.outputs.fault&&e.mode==2&&e.last_injection_us==2000);
            }
        }
        for(unsigned i=0;i<4;++i){assert(rises[i]>=5&&rises[i]<=6&&rises[16+i]>=5&&rises[16+i]<=6);assert(last_width[i]==2000&&last_width[16+i]==2000);}
        if(operation==2)for(unsigned i=1;i<4;++i)assert(e.outputs.pulse[i].start==e.outputs.pulse[0].start);
        tunuino_position_reading no_crank={0};
        tunuino_engine_control_update(&e,clock_us,clock_us,&no_crank,60,1,true,10000,1000,100000,true,92000);
        assert(!mask&&e.outputs.fault==TUNUINO_OUTPUT_SYNC);
        tunuino_engine_control_stop(&e,0);
    }
    /* Both crank-only strategies across 12 outputs and the complete tested RPM
     * range, with missing-tooth gaps, timer wrap and 360-degree event rollover. */
    for(unsigned operation=1;operation<=2;++operation)for(unsigned index=0;index<6;++index){
        unsigned rpm=speeds[index];profile(p);p[0]=2;p[28]=(uint8_t)operation;p[1]=12;
        for(unsigned i=0;i<12;++i){p[32+8*i]=(uint8_t)i;p[33+8*i]=(uint8_t)i;p32(p+34+8*i,i*6000);}
        clock_us=0;tunuino_engine_control_init(&e,gpio,NULL);
        assert(tunuino_engine_control_configure(&e,p,60,2,1));
        uint32_t start=UINT32_MAX-60000u;assert(tunuino_engine_control_arm(&e,start,1));
        memset(rises,0,sizeof(rises));memset(falls,0,sizeof(falls));
        for(uint32_t elapsed=0;elapsed<600000;elapsed+=10){
            clock_us=start+elapsed;tunuino_outputs_poll(&e.outputs,clock_us);
            if(elapsed%250==0){
                unsigned slot=(unsigned)((uint64_t)elapsed*rpm/1000000u),rev=slot/60,tooth=slot%60;if(tooth>57)tooth=57;
                uint32_t edge=start+(uint32_t)(((uint64_t)(rev*60+tooth)*1000000u+rpm/2)/rpm);
                tunuino_position_reading crank_only={1,0,(uint8_t)(rev%2),(uint16_t)tooth,(double)rpm};
                tunuino_engine_control_update(&e,clock_us,edge,&crank_only,60,1,true,10000,1000,100000,true,92000);
                assert(!e.outputs.fault&&e.last_injection_us==2000);
            }
        }
        for(unsigned i=0;i<12;++i)assert(rises[i]>0&&rises[16+i]>0);
        tunuino_engine_control_stop(&e,0);assert(!mask);
    }
    profile(p);p[0]=1;p[28]=1;assert(!tunuino_engine_profile_decode(p,60,2,&cfg));
    p[0]=2;p[28]=3;assert(!tunuino_engine_profile_decode(p,60,2,&cfg));
    puts("Outputs: reset-low, pulse bounds, overlap, watchdog/wrap, sequential timing, 12-cylinder crank-only 360-degree semi/batch with wasted COP across 400-12000 RPM and timer wrap, integrated IAT/voltage/target/ethanol/cylinder corrections, injector-only RPM cuts and latched thermal/voltage protection passed. Synthetic GPIO only.");
    return 0;
}
