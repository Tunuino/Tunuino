/* Copyright (c) 2026 Tunuino. Synthetic host-only IO model; never linked into the ECU. */
#include "tunuino/device.h"
#include "tunuino/link.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
static tunuino_device device;
static uint64_t now;
static bool save(void *u,const uint8_t *p,size_t size){(void)u;now+=2000;return tunuino_device_validate(p,size);}
static uint64_t clock_now(void *u){(void)u;return now;}
int main(void){
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
    fputs("Tunuino IO host test: SYNTHETIC ADC and RPM; no hardware.\n",stderr);
    tunuino_device_init(&device,save,clock_now,NULL);
    const tunuino_identity identity={TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,1,0,15,{1,2,3,4,5,6,7,8,9,10,11,12}};
    const tunuino_dispatch_context context={&identity,&device.live,&device.ve,&device.ignition,&device};
    tunuino_link link;if(tunuino_link_init(&link,&context,1000,1000,now)!=TUNUINO_LINK_RECEIVING)return 1;
    for(;;){
        uint8_t packet[TUNUINO_MAX_BODY+6];int first=getchar();if(first==EOF)break;
        size_t length;
        if(first>16){packet[0]=(uint8_t)first;length=1;}
        else{
            packet[0]=(uint8_t)first;int second=getchar();if(second==EOF)return 2;packet[1]=(uint8_t)second;
            length=(((size_t)packet[0]<<8)|packet[1])+6;
            if(length<7||length>sizeof(packet)||fread(packet+2,1,length-2,stdin)!=length-2)return 3;
        }
        now+=100;uint16_t raw[16];for(unsigned i=0;i<16;++i)raw[i]=2000;
        tunuino_device_sample(&device,now,raw,true,1000000,true,7,0);
        size_t cursor=0;
        while(cursor<length){size_t count=length-cursor;if(count>3)count=3;size_t used;
            tunuino_link_status status=tunuino_link_feed(&link,packet+cursor,count,now,&used);cursor+=used;
            if(status!=TUNUINO_LINK_RECEIVING&&status!=TUNUINO_LINK_RESPONDING)return 4;
        }
        while(link.state==TUNUINO_LINK_RESPONDING){const uint8_t *bytes;size_t count;
            if(tunuino_link_tx_view(&link,now,&bytes,&count)!=TUNUINO_LINK_RESPONDING)return 5;
            if(count>7)count=7;
            if(fwrite(bytes,1,count,stdout)!=count||fflush(stdout)!=0)return 6;
            tunuino_link_status status=tunuino_link_tx_advance(&link,count,now);
            if(status!=TUNUINO_LINK_RECEIVING&&status!=TUNUINO_LINK_RESPONDING)return 7;
        }
    }
    return 0;
}
