/* Copyright (c) 2026 Tunuino. Host-only validation model; no physical flash/IO. */
#include "tunuino/device.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static tunuino_device device;
static unsigned saves;
static bool save(void *u,const uint8_t *p,size_t n){(void)u;(void)p;(void)n;++saves;return true;}
static void put16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void damage(uint8_t *p,unsigned group){
    switch(group){
        case 0:p[5]=1;break;case 1:p[2]=2;break;case 2:p[6]=12;break;case 3:p[7]=12;break;
        case 4:p[8]=5;break;case 5:put16(p+100,2700);break;
        case 10:p[103]=2;break;case 8:p[128]=1;break;case 9:p[102]=2;break;
        case 6:put16(p+256,20001);break;case 7:put16(p+TUNUINO_IO_CONFIG_SIZE+TUNUINO_IO_MAP_SIZE+64,(uint16_t)(int16_t)-9001);break;
    }
}
int main(void){
    tunuino_device_init(&device,save,NULL,NULL);
    uint8_t original[TUNUINO_IO_SETTINGS_SIZE],modified[TUNUINO_IO_SETTINGS_SIZE];memcpy(original,device.active,TUNUINO_IO_SETTINGS_SIZE);
    assert(tunuino_device_validation_errors(NULL,0)==TUNUINO_CONFIG_SCHEMA);
    assert(tunuino_device_validation_errors(original,TUNUINO_IO_SETTINGS_SIZE-1)==TUNUINO_CONFIG_SCHEMA);
    assert(tunuino_device_validation_errors(original,TUNUINO_IO_SETTINGS_SIZE)==0);
    memcpy(modified,original,TUNUINO_IO_SETTINGS_SIZE);put16(modified+14,50001);
    assert(tunuino_device_validation_errors(modified,TUNUINO_IO_SETTINGS_SIZE)==TUNUINO_CONFIG_MAP);
    memcpy(modified,original,TUNUINO_IO_SETTINGS_SIZE);put16(modified+22,5001);
    assert(tunuino_device_validation_errors(modified,TUNUINO_IO_SETTINGS_SIZE)==TUNUINO_CONFIG_LAMBDA);
    for(unsigned group=0;group<11;++group){
        memcpy(modified,original,TUNUINO_IO_SETTINGS_SIZE);damage(modified,group);
        assert(tunuino_device_validation_errors(modified,TUNUINO_IO_SETTINGS_SIZE)==(UINT32_C(1)<<group));
        assert(!tunuino_device_validate(modified,TUNUINO_IO_SETTINGS_SIZE));
    }
    memcpy(modified,original,TUNUINO_IO_SETTINGS_SIZE);for(unsigned group=0;group<8;++group)damage(modified,group);
    memcpy(device.staged,modified,TUNUINO_IO_SETTINGS_SIZE);device.dirty=true;
    assert(tunuino_device_validation_errors(modified,TUNUINO_IO_SETTINGS_SIZE)==255);
    const tunuino_identity identity={TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,2,0,15,{1,2,3,4,5,6,7,8,9,10,11,12}};
    uint8_t packet[128];size_t count,length;const uint8_t *body;
    assert(tunuino_device_reply(&device,&identity,(const uint8_t *)"TUNUINO:V",9,packet,sizeof(packet),&count)==TUNUINO_DISPATCH_OK);
    assert(tunuino_frame_decode(packet,count,&body,&length)==TUNUINO_FRAME_OK);
    assert(length==16&&body[0]==0&&body[1]==1&&body[2]==1&&body[3]==0&&body[7]==255&&body[11]==1);
    assert(memcmp(original,device.active,TUNUINO_IO_SETTINGS_SIZE)==0&&memcmp(modified,device.staged,576)==0&&saves==0&&device.generation==1);
    printf("validation ");for(size_t i=0;i<count;++i)printf("%02x",packet[i]);puts("");
    const uint8_t burn[]={'b',0,1};
    assert(tunuino_device_reply(&device,&identity,burn,3,packet,sizeof(packet),&count)==TUNUINO_DISPATCH_OK);
    assert(tunuino_frame_decode(packet,count,&body,&length)==TUNUINO_FRAME_OK&&length==1&&body[0]==0x84&&saves==0);
    assert(memcmp(original,device.active,TUNUINO_IO_SETTINGS_SIZE)==0&&device.generation==1);
    puts("OK");return 0;
}
