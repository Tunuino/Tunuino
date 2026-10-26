/* Copyright (c) 2026 Tunuino. Proteus v0.7 F427 only.
 * PCB: Tunuino/proteus 383558bf7ca1c53ad8bbd8fdc9dd3a4187013c46.
 * VNLD5160 low-side inputs and MIC4427 non-inverting ignition inputs.
 * IGN is a logic output for an external ignition stage, not a coil power sink.
 * TIM2 is the reference counter; GPT5 provides independent edge/lease IRQs. */
#include "ch.h"
#include "hal.h"
#include "board_io.h"
#include "board_outputs.h"
#include "tunuino/engine_control.h"
#include <string.h>
typedef struct {ioportid_t port;unsigned pad;} pin;
static const pin pins[28]={
    {GPIOD,7},{GPIOG,9},{GPIOG,10},{GPIOG,11},{GPIOG,12},{GPIOG,13},{GPIOG,14},{GPIOB,4},
    {GPIOB,5},{GPIOB,6},{GPIOB,7},{GPIOB,9},{GPIOB,8},{GPIOE,0},{GPIOE,1},{GPIOE,2},
    {GPIOD,4},{GPIOD,3},{GPIOC,9},{GPIOC,8},{GPIOC,7},{GPIOG,8},{GPIOG,7},{GPIOG,6},
    {GPIOG,5},{GPIOG,4},{GPIOG,3},{GPIOG,2}
};
static tunuino_engine_control engine;
static bool timer_ready;
static uint32_t last_host_seen;
static void write_pin(void *user,unsigned channel,bool high){
    (void)user;if(high)palSetPad(pins[channel].port,pins[channel].pad);
    else palClearPad(pins[channel].port,pins[channel].pad);
}
static void timer_schedule(void){
    if(!timer_ready)return;
    if(GPTD5.state!=GPT_READY)gptStopTimerI(&GPTD5);
    uint32_t delay=tunuino_outputs_next(&engine.outputs,TIM2->CNT);
    if(delay)gptStartOneShotI(&GPTD5,delay<5?5:delay);
}
static void alarm_callback(GPTDriver *driver){
    (void)driver;chSysLockFromISR();
    tunuino_outputs_poll(&engine.outputs,TIM2->CNT);
    if(engine.outputs.fault)engine.mode=3;
    timer_schedule();chSysUnlockFromISR();
}
static const GPTConfig timer_config={.frequency=1000000,.callback=alarm_callback,.cr2=0,.dier=0};
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static uint32_t u32(const uint8_t *p){return (uint32_t)tunuino_device_u16(p)<<16|tunuino_device_u16(p+2);}
static bool command(void *user,const uint8_t *q,size_t length,uint8_t *out,size_t capacity,size_t *written){
    tunuino_device *d=user;bool okay=false;
    if(length==1&&q[0]==0&&capacity>=24){
        last_host_seen=TIM2->CNT;
        memset(out,0,24);chSysLock();
        out[1]=1;out[2]=engine.mode;out[3]=engine.outputs.fault;
        put32(out+4,engine.outputs.active_mask);put32(out+8,tunuino_outputs_pending(&engine.outputs));
        out[12]=engine.configured;out[13]=engine.profile.cylinders;out[14]=timer_ready;
        put32(out+16,engine.generation);put32(out+20,engine.last_injection_us);
        chSysUnlock();*written=24;return true;
    }
    if(length==129&&q[0]==1&&!d->dirty){
        chSysLock();okay=tunuino_engine_control_configure(&engine,q+1,d->active[3],d->active[4],d->generation);chSysUnlock();
        if(okay)board_io_phase(d,engine.profile.cam_first,engine.profile.cam_last,engine.profile.cam_phase);
    }else if(length==9&&q[0]==2&&memcmp(q+1,"ARM!",4)==0&&timer_ready&&!d->dirty
        &&d->persisted&&d->active[2]&&u32(q+5)==d->generation){
        chSysLock();last_host_seen=TIM2->CNT;okay=tunuino_engine_control_arm(&engine,TIM2->CNT,d->generation);timer_schedule();chSysUnlock();
    }else if(length==1&&q[0]==3){board_outputs_stop();okay=true;
    }else if(length==8&&q[0]==4&&memcmp(q+4,"TEST",4)==0&&timer_ready
        &&!d->trigger_valid&&!(d->live.valid_mask&TUNUINO_LIVE_RPM_VALID)){
        chSysLock();
        if(!engine.outputs.armed){
            tunuino_outputs_arm(&engine.outputs,TIM2->CNT,100000);
            okay=tunuino_outputs_pulse(&engine.outputs,q[1],TIM2->CNT,TIM2->CNT+1000,tunuino_device_u16(q+2));
            if(okay)engine.mode=4;else tunuino_engine_control_stop(&engine,0);
            timer_schedule();
        }
        chSysUnlock();
    }else if(length==8&&q[0]==5&&memcmp(q+4,"AUX!",4)==0&&timer_ready
        &&!d->trigger_valid&&!(d->live.valid_mask&TUNUINO_LIVE_RPM_VALID)){
        chSysLock();
        unsigned selected=(unsigned)q[1]+1u;
        bool assigned=engine.configured&&!d->dirty&&engine.generation==d->generation
            &&(selected==engine.profile.pump||selected==engine.profile.fan||selected==engine.profile.relay);
        if(assigned){
            okay=tunuino_outputs_aux_test(&engine.outputs,q[1],TIM2->CNT,(uint32_t)tunuino_device_u16(q+2)*1000u);
            if(okay)engine.mode=4;
            timer_schedule();
        }
        chSysUnlock();
    }
    if(okay&&capacity){out[0]=0;*written=1;return true;}
    return false;
}
void board_outputs_start(tunuino_device *d){
    RCC->AHB1ENR|=RCC_AHB1ENR_GPIOBEN|RCC_AHB1ENR_GPIOCEN|RCC_AHB1ENR_GPIODEN|RCC_AHB1ENR_GPIOEEN|RCC_AHB1ENR_GPIOGEN;
    (void)RCC->AHB1ENR;
    /* Write inactive latches before changing pin mode: no startup pulse. */
    for(unsigned i=0;i<28;++i){palClearPad(pins[i].port,pins[i].pad);palSetPadMode(pins[i].port,pins[i].pad,PAL_MODE_OUTPUT_PUSHPULL);}
    tunuino_engine_control_init(&engine,write_pin,NULL);
    timer_ready=gptStart(&GPTD5,&timer_config)==MSG_OK;
    d->output_command=command;d->output_user=d;
}
void board_outputs_stop(void){
    chSysLock();tunuino_engine_control_stop(&engine,0);timer_schedule();chSysUnlock();
}
bool board_outputs_idle(void){bool idle;chSysLock();idle=!engine.outputs.armed;chSysUnlock();return idle;}
void board_outputs_update(tunuino_device *d,const tunuino_position_reading *position,uint32_t edge,uint32_t now){
    uint16_t ve=0;int16_t advance=0;
    bool valid=!d->dirty&&d->adc_valid&&d->trigger_valid;
    if(valid)valid=tunuino_device_engine_values(d,&ve,&advance);
    chSysLock();
    if(engine.configured&&engine.generation!=d->generation){
        tunuino_engine_control_stop(&engine,TUNUINO_OUTPUT_CALIBRATION);engine.configured=0;
    }
    if(engine.mode==4){
        if(d->trigger_valid||(d->live.valid_mask&TUNUINO_LIVE_RPM_VALID))tunuino_engine_control_stop(&engine,TUNUINO_OUTPUT_SYNC);
        else tunuino_outputs_poll(&engine.outputs,now);
        if(!tunuino_outputs_pending(&engine.outputs))tunuino_engine_control_stop(&engine,0);
    }else if(engine.outputs.armed){
        if((uint32_t)(now-last_host_seen)>2000000u)tunuino_engine_control_stop(&engine,TUNUINO_OUTPUT_WATCHDOG);
        else if(d->dirty)tunuino_engine_control_stop(&engine,TUNUINO_OUTPUT_CALIBRATION);
        else tunuino_engine_control_update(&engine,now,edge,position,d->active[3],d->generation,
            valid,ve,advance,d->live.map_milli_kpa,(d->live.valid_mask&TUNUINO_LIVE_COOLANT_VALID)!=0,d->live.coolant_milli_c);
    }
    timer_schedule();chSysUnlock();
}
