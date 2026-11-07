/* Optional native host harness. Build with all C files from Tunuino_Firmware/firmware/src
 * and its firmware/include directory. Simulates storage and idle board outputs,
 * while executing the real firmware dispatcher and configuration validation. */
#include "tunuino/device.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static tunuino_device device;
static uint8_t saved[TUNUINO_IO_SETTINGS_SIZE];
static unsigned saves;
static uint64_t now;
static bool save(void *user,const uint8_t *bytes,size_t size){(void)user;if(size!=sizeof(saved))return false;++saves;memcpy(saved,bytes,size);return true;}
static uint64_t clock_ms(void *user){(void)user;return now;}
static bool output(void *user,const uint8_t *q,size_t n,uint8_t *out,size_t cap,size_t *written){
    (void)user;if(n!=1||q[0]!=0||cap<24)return false;memset(out,0,24);out[1]=1;*written=24;return true;
}
int main(int argc,char **argv){
    (void)argv;
    tunuino_identity id={TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,2,0,207,{42}};
    tunuino_device_init(&device,save,clock_ms,NULL);device.output_command=output;
    if(argc>1){uint8_t config[TUNUINO_IO_SETTINGS_SIZE];memcpy(config,device.active,sizeof(config));uint8_t *p=config+128;
        p[0]=1;p[1]=4;const unsigned at[]={2,4,6,8,12,14};const unsigned v[]={5000,2000,1000,15000,9000,8500};
        for(unsigned i=0;i<6;++i){p[at[i]]=(uint8_t)(v[i]>>8);p[at[i]+1]=(uint8_t)v[i];}
        const unsigned angle[]={0,54000,18000,36000};
        for(unsigned i=0;i<4;++i){p[32+8*i]=p[33+8*i]=(uint8_t)i;p[36+8*i]=(uint8_t)(angle[i]>>8);p[37+8*i]=(uint8_t)angle[i];}
        if(!tunuino_device_restore(&device,config,sizeof(config)))return 5;
    }
    tunuino_dispatch_context context={&id,&device.live,&device.ve,&device.ignition,&device};
    char line[16384];uint8_t q[4096],packet[4102],response[4102];uint16_t raw[16]={0};raw[11]=1619;
    while(fgets(line,sizeof(line),stdin)){
        size_t length=strcspn(line,"\r\n");if(length%2||length/2>sizeof(q))return 2;
        for(size_t i=0;i<length/2;++i){char byte[3]={line[2*i],line[2*i+1],0};q[i]=(uint8_t)strtoul(byte,NULL,16);}
        size_t encoded=0,written=0;const uint8_t *body;size_t size;
        tunuino_device_sample(&device,++now,raw,true,0,false,0,0);
        if(tunuino_frame_encode(q,length/2,packet,sizeof(packet),&encoded)!=TUNUINO_FRAME_OK
            ||tunuino_dispatch_reply(&context,packet,encoded,response,sizeof(response),&written)!=TUNUINO_DISPATCH_OK
            ||tunuino_frame_decode(response,written,&body,&size)!=TUNUINO_FRAME_OK)return 3;
        for(size_t i=0;i<size;++i){printf("%02x",body[i]);}
        putchar('\n');fflush(stdout);
    }
    return saves?0:4;
}
