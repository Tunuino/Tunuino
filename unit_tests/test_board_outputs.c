/* Copyright (c) 2026 Tunuino. Actual board adapter with synthetic GPIO/GPT. */
#include "hal.h"
#include "board_outputs.h"
#include "tunuino/frame.h"
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
    puts("F427 output adapter: actual framed commands, reset GPIO levels, LS/IGN pin routing, GPT IRQ pulse edges, arm gates, profile phase wiring, main-loop stall and host lease cut passed. Register model only.");
    return 0;
}
#else
int main(void){
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
    /* Keep standalone adapter checks linked and exercised before serving. */
    (void)irq;(void)off;(void)p16;(void)request;
    tunuino_device d;tunuino_device_init(&d,NULL,NULL,NULL);board_outputs_start(&d);
    d.persisted=true;d.active[2]=1;
    const tunuino_identity id={TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,2,0,15,{1}};
    for(;;){int hi=getchar();if(hi==EOF)break;int lo=getchar();if(lo==EOF)return 1;
        size_t total=((unsigned)hi<<8|(unsigned)lo)+6;uint8_t frame[TUNUINO_MAX_BODY+6];
        if(total<7||total>sizeof(frame))return 2;
        frame[0]=(uint8_t)hi;frame[1]=(uint8_t)lo;if(fread(frame+2,1,total-2,stdin)!=total-2)return 3;
        const uint8_t *q;size_t bytes;if(tunuino_frame_decode(frame,total,&q,&bytes)!=TUNUINO_FRAME_OK)return 4;
        TIM2->CNT+=100;
        if(tunuino_device_reply(&d,&id,q,bytes,response,sizeof(response),&response_size)!=TUNUINO_DISPATCH_OK)return 5;
        if(fwrite(response,1,response_size,stdout)!=response_size||fflush(stdout))return 6;
    }
    return 0;
}
#endif
