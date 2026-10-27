/* Copyright (c) 2026 Tunuino. Actual board adapter with synthetic GPIO/GPT. */
#include "hal.h"
#include "board_outputs.h"
#include "tunuino/frame.h"
#include "tunuino/live_data.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
gpio_model output_gpio[5];timer_model output_clock;rcc_model output_rcc;GPTDriver GPTD5;
static const GPTConfig *timer_config;
static uint32_t alarm_at;
static unsigned phase_calls;
void palSetPad(ioportid_t p,unsigned pad){assert(p->configured&(1u<<pad));p->value|=1u<<pad;}
void palClearPad(ioportid_t p,unsigned pad){p->value&=~(1u<<pad);}
void palSetPadMode(ioportid_t p,unsigned pad,unsigned mode){assert(mode==1&&!(p->value&(1u<<pad)));p->configured|=1u<<pad;}
msg_t gptStart(GPTDriver *d,const GPTConfig *config){timer_config=config;assert(config->frequency==1000000);d->state=GPT_READY;return MSG_OK;}
void gptStopTimerI(GPTDriver *d){d->state=GPT_READY;}
void gptStartOneShotI(GPTDriver *d,uint32_t delay){assert(d->state==GPT_READY&&delay>0);alarm_at=TIM2->CNT+delay;d->state=GPT_ONESHOT;}
void board_io_phase(tunuino_device *d,uint8_t first,uint8_t last,uint8_t phase){(void)d;assert(first==1&&last==2&&phase==0);++phase_calls;}
static void irq(void){assert(GPTD5.state==GPT_ONESHOT);TIM2->CNT=alarm_at;GPTD5.state=GPT_READY;timer_config->callback(&GPTD5);}
static void off(void){for(unsigned i=0;i<5;++i)assert(output_gpio[i].value==0);}
static void p16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static uint8_t response[300];static size_t response_size;
static const uint8_t *request(tunuino_device *d,const uint8_t *op,size_t count){
    uint8_t q[160];memcpy(q,"TUNUINO:X",9);memcpy(q+9,op,count);
    tunuino_identity id={TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,2,0,15,{1}};
    assert(tunuino_device_reply(d,&id,q,count+9,response,sizeof(response),&response_size)==TUNUINO_DISPATCH_OK);
    const uint8_t *body;size_t bytes;assert(tunuino_frame_decode(response,response_size,&body,&bytes)==TUNUINO_FRAME_OK);return body;
}
#ifndef TUNUINO_OUTPUT_PEER
#include "aux_fixture.h"
int main(void){
    tunuino_device d;tunuino_device_init(&d,NULL,NULL,NULL);board_outputs_start(&d);off();assert(board_outputs_idle());
    unsigned configured=0;for(unsigned i=0;i<5;++i)for(unsigned bit=0;bit<16;++bit)configured+=(output_gpio[i].configured>>bit)&1u;
    assert(configured==28);
    const uint8_t status[]={0},stop[]={3};const uint8_t *r=request(&d,status,1);assert(r[1]==1&&r[2]==0&&r[14]==1);
    uint8_t pulse[]={4,0,3,232,'T','E','S','T'};
    assert(request(&d,pulse,8)[0]==0);assert(!board_outputs_idle());irq();assert(GPIOD->value==(1u<<7));irq();off();
    tunuino_position_reading position={0};board_outputs_update(&d,&position,0,TIM2->CNT);assert(board_outputs_idle());
    pulse[1]=16;assert(request(&d,pulse,8)[0]==0);irq();assert(GPIOD->value==(1u<<4));irq();off();board_outputs_update(&d,&position,0,TIM2->CNT);
    pulse[2]=0x13;pulse[3]=0x89;assert(request(&d,pulse,8)[0]==0x84);off();assert(board_outputs_idle());
    d.trigger_valid=true;pulse[2]=3;pulse[3]=232;assert(request(&d,pulse,8)[0]==0x84);d.trigger_valid=false;
    uint8_t config[129]={1,1,1};uint8_t *p=config+1;
    p16(p+2,3000);p16(p+4,2000);p16(p+6,500);p16(p+8,15000);p16(p+12,9500);p16(p+14,9000);
    p[24]=16;p[25]=1;p[26]=2;p16(p+36,18000); // cylinder PMS 18000 centidegrees, big-endian U32
    assert(request(&d,config,129)[0]==0&&phase_calls==1);
    /* Relay test is available only for explicitly assigned auxiliaries, never injector/IGN channels. */
    uint8_t aux[]={5,15,1,244,'A','U','X','!'}; /* LS16, 500 ms */
    assert(request(&d,aux,8)[0]==0);irq();assert(GPIOE->value==(1u<<2));
    uint32_t relay_start=TIM2->CNT;irq();assert(TIM2->CNT-relay_start==500000);off();
    board_outputs_update(&d,&position,0,TIM2->CNT);assert(board_outputs_idle());
    aux[1]=0;assert(request(&d,aux,8)[0]==0x84);off(); /* LS1 is an injector */
    aux[1]=16;assert(request(&d,aux,8)[0]==0x84);off(); /* IGN never accepts relay timings */
    aux[1]=15;aux[2]=3;aux[3]=33;assert(request(&d,aux,8)[0]==0x84);off(); /* 801 ms */
    aux[2]=1;aux[3]=244;d.trigger_valid=true;assert(request(&d,aux,8)[0]==0x84);d.trigger_valid=false;
    d.dirty=true;assert(request(&d,aux,8)[0]==0x84);d.dirty=false;
    ++d.generation;assert(request(&d,aux,8)[0]==0x84);--d.generation;
    assert(request(&d,config,129)[0]==0);
    assert(request(&d,aux,8)[0]==0);irq();d.trigger_valid=true;
    board_outputs_update(&d,&position,0,TIM2->CNT);off();assert(board_outputs_idle());d.trigger_valid=false;
    assert(request(&d,stop,1)[0]==0);
    assert(request(&d,aux,8)[0]==0);irq();assert(request(&d,stop,1)[0]==0);off();
    uint8_t arm[]={2,'A','R','M','!',0,0,0,1};assert(request(&d,arm,9)[0]==0x84);
    d.persisted=true;d.active[2]=1;
    assert(request(&d,arm,9)[0]==0);board_outputs_update(&d,&position,0,TIM2->CNT);assert(GPIOB->value==0&&GPIOC->value==0);
    assert(GPIOG->value==0);assert(GPIOD->value==0); // relay LS16 is PE2
    assert(GPIOE->value==(1u<<2));
    irq();off();r=request(&d,status,1);assert(r[2]==3&&(r[3]&2));
    assert(request(&d,stop,1)[0]==0);assert(request(&d,arm,9)[0]==0);
    for(unsigned i=0;i<405;++i){TIM2->CNT+=5000;board_outputs_update(&d,&position,0,TIM2->CNT);}
    off();r=request(&d,status,1);assert(r[2]==3&&(r[3]&2));
    d.adc_valid=d.trigger_valid=true;d.live.valid_mask=3;d.live.rpm_milli=200000;d.live.map_milli_kpa=100000;
    for(unsigned i=0;i<256;++i){p16(d.active+256+64+2*i,10000);p16(d.active+256+576+64+2*i,1000);}
    uint16_t ve;int16_t advance;
    assert(tunuino_device_engine_values(&d,&ve,&advance)&&ve==10000&&advance==1000);
    d.live.map_milli_kpa=300000;assert(!tunuino_device_engine_values(&d,&ve,&advance));
    /* Persisted autonomous profile: no host lease, no arm on save/reconnect,
     * stop/fault latch, actual timed outputs and rejection of running writes. */
    board_outputs_stop();d.live.map_milli_kpa=100000;d.live.coolant_milli_c=90000;
    d.active[6]=1;d.active[8]=1;p16(d.active+16,20000);p16(d.active+64,10000);
    memcpy(d.active+128,p,128);d.active[102]=1;
    assert(tunuino_device_validate(d.active,sizeof(d.active)));
    assert(tunuino_device_restore(&d,d.active,sizeof(d.active)));
    board_outputs_restore(&d,false);r=request(&d,status,1);assert(r[2]==0&&r[15]==3);off();
    board_outputs_restore(&d,true);off();
    memset(&position,0,sizeof(position));
    for(unsigned i=0;i<500;++i){TIM2->CNT+=5000;board_outputs_update(&d,&position,0,TIM2->CNT);}
    r=request(&d,status,1);assert(r[2]==1&&r[3]==0&&r[15]==3); /* >2s, no USB */
    uint8_t write[]={'w',0,1,0,96,0,2,0x17,0x70};
    tunuino_identity identity={TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,2,0,15,{1}};
    assert(tunuino_device_reply(&d,&identity,write,sizeof(write),response,sizeof(response),&response_size)==TUNUINO_DISPATCH_OK);
    const uint8_t *body;size_t bytes;assert(tunuino_frame_decode(response,response_size,&body,&bytes)==TUNUINO_FRAME_OK);
    assert(body[0]==0x84&&!d.dirty); /* refused before any staged mutation */
    d.adc_valid=d.trigger_valid=true;d.live.valid_mask=15;d.live.rpm_milli=3000000;
    uint32_t origin=TIM2->CNT;
    for(unsigned step=0;step<1000;++step){
        uint32_t next=origin+step*100u;
        if(GPTD5.state==GPT_ONESHOT&&(int32_t)(next-alarm_at)>=0)irq();
        TIM2->CNT=next;uint32_t elapsed=next-origin,phase=elapsed%20000u;
        position.crank_valid=position.cycle_valid=true;position.rpm=3000;
        position.revolution_phase=(elapsed/20000u)%2;position.tooth_index=(uint16_t)(phase*36u/20000u);
        if(position.tooth_index>34)position.tooth_index=34;
        uint32_t edge=next-phase+(uint32_t)position.tooth_index*20000u/36u;
        board_outputs_update(&d,&position,edge,next);
    }
    r=request(&d,status,1);assert(r[2]==2&&r[3]==0);
    position.cycle_valid=false;board_outputs_update(&d,&position,TIM2->CNT,TIM2->CNT);off();
    assert(request(&d,status,1)[2]==3);
    position.cycle_valid=true;board_outputs_update(&d,&position,TIM2->CNT,TIM2->CNT);off();
    assert(request(&d,status,1)[2]==3); /* no automatic restart after fault */
    request(&d,stop,1);board_outputs_update(&d,&position,TIM2->CNT,TIM2->CNT);off();
    assert(request(&d,status,1)[2]==0);
    const uint8_t activate[]={6,'R','U','N','!'};assert(request(&d,activate,5)[0]==0);
    assert(request(&d,status,1)[2]==1);request(&d,stop,1);off();
    /* Persisted page 4 drives actual PWM pins and latches overboost cut. */
    uint8_t aux_config[256];aux_fixture(aux_config);aux_config[1]=1;aux_config[2]=8;aux_config[3]=2;aux_config[4]=9;aux_config[5]=3;
    memcpy(d.active+TUNUINO_IO_AUX_OFFSET,aux_config,256);assert(tunuino_device_validate(d.active,sizeof(d.active)));
    assert(tunuino_device_restore(&d,d.active,sizeof(d.active)));board_outputs_restore(&d,false);
    d.adc_valid=true;d.raw[2]=400;d.live.valid_mask=14;d.live.rpm_milli=0;d.live.map_milli_kpa=100000;d.live.coolant_milli_c=20000;
    memset(&position,0,sizeof(position));assert(request(&d,activate,5)[0]==0);
    board_outputs_update(&d,&position,TIM2->CNT,TIM2->CNT);const uint8_t aux_state[]={7};r=request(&d,aux_state,1);
    assert(r[4]==1&&tunuino_device_u16(r+6)==0&&tunuino_device_u16(r+8)==3500);
    irq();assert(GPIOB->value&(1u<<4));irq();assert(!(GPIOB->value&(1u<<4))); /* LS8 duty */
    d.raw[2]=0;board_outputs_update(&d,&position,TIM2->CNT,TIM2->CNT);r=request(&d,aux_state,1);
    assert(r[4]==0&&tunuino_device_u16(r+8)==0); /* invalid TPS commands safe OFF */
    d.live.map_milli_kpa=200000;board_outputs_update(&d,&position,TIM2->CNT,TIM2->CNT);off();
    r=request(&d,status,1);assert(r[2]==3&&(r[3]&16));
    d.live.map_milli_kpa=100000;board_outputs_update(&d,&position,TIM2->CNT,TIM2->CNT);off();assert(request(&d,status,1)[2]==3);
    request(&d,stop,1);
    puts("F427 output adapter: actual framed commands, reset GPIO levels, LS/IGN pin routing, GPT IRQ pulse edges, arm gates, profile phase wiring, main-loop stall and host lease cut passed. Register model only.");
    return 0;
}
#else
static bool save_profile(void *user,const uint8_t *bytes,size_t length){
    (void)user;return length==TUNUINO_IO_SETTINGS_SIZE&&tunuino_device_validate(bytes,length)&&board_outputs_idle();
}
int main(void){
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
    /* Keep standalone adapter checks linked and exercised before serving. */
    (void)irq;(void)off;(void)p16;(void)request;
    tunuino_device d;tunuino_device_init(&d,save_profile,NULL,NULL);board_outputs_start(&d);
    d.persisted=true;d.active[2]=1;
    memcpy(d.staged,d.active,sizeof(d.staged));
    const tunuino_identity id={TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,2,0,15,{1}};
    for(;;){int hi=getchar();if(hi==EOF)break;int lo=getchar();if(lo==EOF)return 1;
        size_t total=((unsigned)hi<<8|(unsigned)lo)+6;uint8_t frame[TUNUINO_MAX_BODY+6];
        if(total<7||total>sizeof(frame))return 2;
        frame[0]=(uint8_t)hi;frame[1]=(uint8_t)lo;if(fread(frame+2,1,total-2,stdin)!=total-2)return 3;
        const uint8_t *q;size_t bytes;if(tunuino_frame_decode(frame,total,&q,&bytes)!=TUNUINO_FRAME_OK)return 4;
        TIM2->CNT+=100;
        /* Match the production dispatcher: partial telemetry and native output commands share one stream. */
        if(bytes==9&&memcmp(q,"TUNUINO:P",9)==0){
            ++d.live.time_ms;
            if(tunuino_live_partial_reply(&d.live,response,sizeof(response),&response_size)!=TUNUINO_LIVE_OK)return 5;
        }else if(tunuino_device_reply(&d,&id,q,bytes,response,sizeof(response),&response_size)!=TUNUINO_DISPATCH_OK)return 5;
        if(fwrite(response,1,response_size,stdout)!=response_size||fflush(stdout))return 6;
    }
    return 0;
}
#endif
