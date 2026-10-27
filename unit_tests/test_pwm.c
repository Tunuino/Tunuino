/* Copyright (c) 2026 Tunuino. PWM shares IRQ deadlines with injector pulses. */
#include "tunuino/outputs.h"
#include <assert.h>
#include <stdio.h>
static uint32_t mask;
static void gpio(void *u,unsigned channel,bool high){(void)u;if(high)mask|=1u<<channel;else mask&=~(1u<<channel);}
int main(void){
    tunuino_outputs o;tunuino_outputs_init(&o,gpio,NULL);
    assert(!tunuino_outputs_pwm(&o,8,0,50,2500));tunuino_outputs_arm(&o,0,20000);
    assert(tunuino_outputs_pwm(&o,8,0,50,2500));assert(!tunuino_outputs_pulse(&o,8,0,1000,1000));assert(!tunuino_outputs_level(&o,8,true));
    assert(tunuino_outputs_pulse(&o,0,0,2000,3000));assert(tunuino_outputs_next(&o,0)==1000);
    tunuino_outputs_poll(&o,1000);assert(mask==(1u<<8));
    tunuino_outputs_poll(&o,2000);assert(mask==((1u<<8)|1u));tunuino_outputs_poll(&o,5000);assert(mask==(1u<<8));
    tunuino_outputs_poll(&o,6000);assert(!mask);assert(tunuino_outputs_keepalive(&o,15000,20000));
    assert(tunuino_outputs_pwm(&o,8,15000,50,5000));tunuino_outputs_poll(&o,21000);assert(mask==(1u<<8));
    assert(tunuino_outputs_pwm(&o,8,22000,50,0));assert(!mask); /* immediate safe OFF */
    assert(!tunuino_outputs_pwm(&o,8,22000,100,5000));assert(!tunuino_outputs_pwm(&o,16,22000,50,5000));
    tunuino_outputs_poll(&o,35000);assert(!mask&&!o.armed&&o.fault==TUNUINO_OUTPUT_WATCHDOG);
    tunuino_outputs_arm(&o,UINT32_MAX-500u,20000);assert(tunuino_outputs_pwm(&o,9,UINT32_MAX-500u,100,10000));
    tunuino_outputs_poll(&o,499);assert(mask==(1u<<9));tunuino_outputs_poll(&o,10499);assert(mask==(1u<<9));
    tunuino_outputs_stop(&o,0);assert(!mask&&!tunuino_outputs_pending(&o));
    tunuino_outputs_arm(&o,0,20000);assert(tunuino_outputs_pwm(&o,8,0,100,5000));tunuino_outputs_poll(&o,1101);assert(!mask&&!o.armed&&(o.fault&TUNUINO_OUTPUT_LATE));
    puts("PWM: exact period/duty edges, concurrent injector, ownership, immediate OFF, watchdog, late IRQ and wrap passed. GPIO model only.");return 0;
}
