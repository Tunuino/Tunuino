/* Copyright (c) 2026 Tunuino. */
#include "tunuino/engine_control.h"
#include <math.h>
#include <string.h>
static uint16_t u16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]<<8|p[1]);}
static uint32_t u32(const uint8_t *p){return (uint32_t)u16(p)<<16|u16(p+2);}
static int32_t signed16(const uint8_t *p){uint16_t v=u16(p);return v<32768?v:(int32_t)v-65536;}
static uint32_t angle(int64_t value){int64_t a=value%72000;return (uint32_t)(a<0?a+72000:a);}
bool tunuino_engine_profile_decode(const uint8_t p[128],uint16_t slots,uint8_t missing,tunuino_engine_profile *out){
    if(!p||!out||p[0]!=1||p[1]<1||p[1]>12||slots<4||slots>120||missing==0||missing>=slots/2)return false;
    tunuino_engine_profile c={0};c.cylinders=p[1];
    c.required_fuel_us=u16(p+2);c.dwell_us=u16(p+4);c.dead_us=u16(p+6);
    c.maximum_injection_us=u16(p+8);c.prime_ms=u16(p+10);
    c.fan_on_centi=(int16_t)signed16(p+12);c.fan_off_centi=(int16_t)signed16(p+14);
    c.injection_end_centi=(int16_t)signed16(p+16);c.tooth_zero_centi=u32(p+18);
    c.pump=p[22];c.fan=p[23];c.relay=p[24];c.cam_first=p[25];c.cam_last=p[26];c.cam_phase=p[27];
    if(c.required_fuel_us<100||c.required_fuel_us>30000||c.dwell_us<100||c.dwell_us>5000
        ||c.dead_us>5000||c.maximum_injection_us<100||c.maximum_injection_us>20000
        ||c.dead_us>=c.maximum_injection_us||c.prime_ms>3000||c.tooth_zero_centi>=72000
        ||c.injection_end_centi< -30000||c.injection_end_centi>30000
        ||c.fan_off_centi<0||c.fan_off_centi>=c.fan_on_centi||c.fan_on_centi>12000
        ||c.cam_first>c.cam_last||c.cam_last>=slots-missing||c.cam_phase>1)return false;
    for(unsigned i=28;i<32;++i)if(p[i])return false;
    uint32_t used=0;
    for(unsigned i=0;i<12;++i){const uint8_t *row=p+32+8*i;
        if(i>=c.cylinders){for(unsigned j=0;j<8;++j)if(row[j])return false;continue;}
        if(row[0]>=16||row[1]>=12||u32(row+2)>=72000||row[6]||row[7])return false;
        uint32_t mask=(UINT32_C(1)<<row[0])|(UINT32_C(1)<<(16+row[1]));
        if(used&mask)return false;
        used|=mask;c.cylinder[i]=(tunuino_cylinder){row[0],row[1],u32(row+2)};
        for(unsigned j=0;j<i;++j)if(c.cylinder[j].tdc_centi==c.cylinder[i].tdc_centi)return false;
    }
    const uint8_t aux[3]={c.pump,c.fan,c.relay};
    for(unsigned i=0;i<3;++i)if(aux[i]){
        if(aux[i]>16||(used&(UINT32_C(1)<<(aux[i]-1))))return false;
        used|=UINT32_C(1)<<(aux[i]-1);
    }
    *out=c;return true;
}
void tunuino_engine_control_init(tunuino_engine_control *e,tunuino_output_write write,void *user){
    memset(e,0,sizeof(*e));tunuino_outputs_init(&e->outputs,write,user);
}
void tunuino_engine_control_stop(tunuino_engine_control *e,uint8_t fault){
    tunuino_outputs_stop(&e->outputs,fault);e->mode=fault?3:0;e->have_phase=0;e->fan_on=0;e->last_injection_us=0;
}
bool tunuino_engine_control_configure(tunuino_engine_control *e,const uint8_t p[128],uint16_t slots,uint8_t missing,uint32_t generation){
    tunuino_engine_profile profile;
    if(e->outputs.armed||!generation||!tunuino_engine_profile_decode(p,slots,missing,&profile))return false;
    e->profile=profile;e->configured=1;e->generation=generation;e->missing=missing;return true;
}
bool tunuino_engine_control_arm(tunuino_engine_control *e,uint32_t now,uint32_t generation){
    if(e->outputs.armed||!e->configured||generation!=e->generation)return false;
    tunuino_outputs_arm(&e->outputs,now,20000);e->mode=1;e->armed_at=e->last_now=now;
    e->cycle=0;e->have_phase=0;e->fan_on=0;e->last_injection_us=0;
    for(unsigned i=0;i<28;++i)e->scheduled_cycle[i]=UINT32_MAX;
    return e->outputs.armed!=0;
}
static void auxiliary(tunuino_engine_control *e,uint32_t now,bool crank,bool clt_valid,int32_t clt){
    const tunuino_engine_profile *p=&e->profile;
    bool prime=(uint32_t)(now-e->armed_at)<(uint32_t)p->prime_ms*1000u;
    if(p->pump)(void)tunuino_outputs_level(&e->outputs,p->pump-1,prime||crank);
    if(p->relay)(void)tunuino_outputs_level(&e->outputs,p->relay-1,true);
    if(!clt_valid||clt>=(int32_t)p->fan_on_centi*10)e->fan_on=1;
    else if(clt<=(int32_t)p->fan_off_centi*10)e->fan_on=0;
    if(p->fan)(void)tunuino_outputs_level(&e->outputs,p->fan-1,e->fan_on!=0);
}
static bool schedule(tunuino_engine_control *e,unsigned ch,uint32_t target,uint32_t duration,
    uint32_t current,uint32_t revolution,uint32_t now,bool first){
    uint32_t cycle=e->cycle+(target<=current?1u:0u);
    if(e->scheduled_cycle[ch]==cycle)return true;
    uint32_t delta=(target+72000u-current)%72000u;
    if(!delta)delta=72000;
    uint32_t until=(uint32_t)(((uint64_t)delta*revolution+18000)/36000u);
    if(until<=duration+10u){
        if(first){e->scheduled_cycle[ch]=cycle;return true;}
        return false;
    }
    uint32_t start=until-duration;
    if(start>3000u)return true;
    if(!tunuino_outputs_pulse(&e->outputs,ch,now,now+start,duration))return false;
    e->scheduled_cycle[ch]=cycle;return true;
}
void tunuino_engine_control_update(tunuino_engine_control *e,uint32_t now,uint32_t edge,
    const tunuino_position_reading *r,uint16_t slots,uint32_t generation,
    bool valid,uint16_t ve,int16_t advance,uint32_t map,bool clt_valid,int32_t clt){
    if(!e->outputs.armed)return;
    if(!tunuino_outputs_keepalive(&e->outputs,now,20000)){e->mode=3;return;}
    if(generation!=e->generation){tunuino_engine_control_stop(e,TUNUINO_OUTPUT_CALIBRATION);e->configured=0;return;}
    bool crank=r&&r->crank_valid;
    if(!r||!r->cycle_valid||!crank||!valid){
        if(e->have_phase){tunuino_engine_control_stop(e,TUNUINO_OUTPUT_SYNC);return;}
        auxiliary(e,now,crank,clt_valid,clt);return;
    }
    if(!clt_valid||slots<4||slots>120||!isfinite(r->rpm)||r->rpm<30||r->rpm>20000
        ||r->revolution_phase>1||r->tooth_index>=slots||ve==0||ve>30000
        ||advance< -9000||advance>9000||map==0||map>500000){
        tunuino_engine_control_stop(e,TUNUINO_OUTPUT_CALIBRATION);return;
    }
    uint32_t revolution=(uint32_t)(60000000.0/r->rpm+0.5),age=now-edge;
    uint32_t maximum_age=(uint32_t)((uint64_t)revolution*(e->missing+1u)*2u/slots);
    if(age>maximum_age||age>revolution/2u){tunuino_engine_control_stop(e,TUNUINO_OUTPUT_SYNC);return;}
    uint32_t current=angle((int64_t)e->profile.tooth_zero_centi+(uint32_t)r->revolution_phase*36000u
        +(uint32_t)r->tooth_index*36000u/slots+(uint64_t)age*36000u/revolution);
    bool first=!e->have_phase;
    if(!first){
        uint32_t delta=(current+72000u-e->last_angle)%72000u;
        if((uint32_t)(now-e->last_now)>revolution||delta>36000u){
            if(current<e->last_angle&&e->last_angle-current<100u)current=e->last_angle;
            else {tunuino_engine_control_stop(e,TUNUINO_OUTPUT_SYNC);return;}
        }
        if(current<e->last_angle)++e->cycle;
    }
    e->have_phase=1;e->mode=2;e->last_angle=current;e->last_now=now;
    uint64_t fuel=(uint64_t)e->profile.required_fuel_us*ve*map;
    uint32_t injection=(uint32_t)((fuel+500000000u)/1000000000u)+e->profile.dead_us;
    if(injection==0||injection>e->profile.maximum_injection_us
        ||injection>(uint64_t)revolution*16u/10u||e->profile.dwell_us>(uint64_t)revolution*16u/10u){
        tunuino_engine_control_stop(e,TUNUINO_OUTPUT_CALIBRATION);return;
    }
    e->last_injection_us=injection;
    for(unsigned i=0;i<e->profile.cylinders;++i){const tunuino_cylinder *c=&e->profile.cylinder[i];
        if(!schedule(e,c->injector,angle((int64_t)c->tdc_centi+e->profile.injection_end_centi),injection,current,revolution,now,first)
            ||!schedule(e,16+c->coil,angle((int64_t)c->tdc_centi-advance),e->profile.dwell_us,current,revolution,now,first)){
            tunuino_engine_control_stop(e,TUNUINO_OUTPUT_LATE);return;
        }
    }
    auxiliary(e,now,crank,clt_valid,clt);
}
