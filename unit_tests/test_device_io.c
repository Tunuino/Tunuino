/* Copyright (c) 2026 Tunuino. Host model: no physical ADC, flash or USB. */
#include "tunuino/device.h"
#include "tunuino/config_journal.h"
#include "tunuino/link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t flash[2][TUNUINO_JOURNAL_BANK_BYTES],baseline[2][TUNUINO_JOURNAL_BANK_BYTES];
static int remaining=-1;
static unsigned erased_banks,programmed;
static tunuino_config_journal journal;
static tunuino_device device;
static uint64_t clock_ms;
static uint8_t response[512];static size_t response_size;

static bool read_flash(void *u,unsigned b,size_t o,uint8_t *p,size_t n){(void)u;if(b>1||o+n>TUNUINO_JOURNAL_BANK_BYTES)return false;memcpy(p,flash[b]+o,n);return true;}
static bool program_flash(void *u,unsigned b,size_t o,const uint8_t *p,size_t n){
    (void)u;assert(b<2&&o%4==0&&n%4==0&&o+n<=TUNUINO_JOURNAL_BANK_BYTES);
    for(size_t i=0;i<n;i+=4){if(remaining==0)return false;if(remaining>0)--remaining;
        for(unsigned j=0;j<4;++j){assert(flash[b][o+i+j]==255);flash[b][o+i+j]&=p[i+j];}++programmed;}
    return true;
}
static bool erase_flash(void *u,unsigned b){(void)u;assert(b<2);memset(flash[b],255,sizeof(flash[b]));++erased_banks;return true;}
static bool save(void *u,const uint8_t *p,size_t n){(void)u;clock_ms+=2000;return n==576&&tunuino_journal_save(&journal,p);}
static uint64_t clock_now(void *u){(void)u;return clock_ms;}
static void put16(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static const tunuino_journal_io io={read_flash,program_flash,erase_flash,NULL};
static tunuino_identity identity={TUNUINO_PROTEUS,TUNUINO_STM32_F4,7,0,1,0,15,{1,2,3,4,5,6,7,8,9,10,11,12}};
static tunuino_dispatch_context context={&identity,&device.live,&device.ve,&device.ignition,&device};

static const uint8_t *query(const uint8_t *bytes,size_t size,size_t *length){
    uint8_t packet[512];size_t count;
    assert(tunuino_frame_encode(bytes,size,packet,sizeof(packet),&count)==TUNUINO_FRAME_OK);
    assert(tunuino_dispatch_reply(&context,packet,count,response,sizeof(response),&response_size)==TUNUINO_DISPATCH_OK);
    const uint8_t *body;assert(tunuino_frame_decode(response,response_size,&body,length)==TUNUINO_FRAME_OK);return body;
}
static void vector(const char *name){printf("%s ",name);for(size_t i=0;i<response_size;++i)printf("%02x",response[i]);puts("");}
static void write_page(unsigned page,const uint8_t *bytes,size_t size){
    for(size_t offset=0;offset<size;){size_t n=size-offset;if(n>64)n=64;uint8_t q[71]={'w',0,0,0,0,0,0};size_t length;
        put16(q+1,page);put16(q+3,(unsigned)offset);put16(q+5,(unsigned)n);memcpy(q+7,bytes+offset,n);
        const uint8_t *body=query(q,7+n,&length);assert(length==1&&body[0]==0);offset+=n;}
}
static void burn(unsigned status){const uint8_t q[]={'b',0,1};size_t size;const uint8_t *p=query(q,3,&size);assert(size==1&&p[0]==status);}

static void persistence(void){
    uint8_t settings[576];memset(flash,255,sizeof(flash));
    assert(!tunuino_journal_init(&journal,&io,settings));
    assert(tunuino_journal_save(&journal,device.active));assert(journal.sequence==1);
    memcpy(baseline,flash,sizeof(flash));memcpy(settings,device.active,576);put16(settings+100,3290);
    for(int cut=0;cut<=152;++cut){
        memcpy(flash,baseline,sizeof(flash));remaining=-1;
        tunuino_config_journal recovery;uint8_t loaded[576];assert(tunuino_journal_init(&recovery,&io,loaded));
        remaining=cut;bool okay=tunuino_journal_save(&recovery,settings);remaining=-1;
        assert(tunuino_journal_init(&recovery,&io,loaded));
        assert(memcmp(loaded,okay?settings:device.active,576)==0);
    }
    memcpy(flash,baseline,sizeof(flash));assert(tunuino_journal_init(&journal,&io,settings));
    for(unsigned i=0;i<230;++i){put16(settings+100,3200+i);assert(tunuino_journal_save(&journal,settings));}
    assert(erased_banks>=1&&journal.bank==1);
    tunuino_config_journal recovery;uint8_t restored[576];assert(tunuino_journal_init(&recovery,&io,restored));
    assert(memcmp(restored,settings,576)==0);
    unsigned before=programmed;assert(tunuino_journal_save(&recovery,settings));assert(programmed==before);
    /* Corrupt the newest committed record: restart selects the previous one. */
    flash[recovery.bank][recovery.offset+22]^=1;
    assert(tunuino_journal_init(&recovery,&io,restored));assert(memcmp(restored,settings,576)!=0);
}

int main(void){
    memset(flash,255,sizeof(flash));uint8_t loaded[576];(void)tunuino_journal_init(&journal,&io,loaded);
    tunuino_device_init(&device,save,clock_now,NULL);assert(tunuino_device_validate(device.active,576));
    assert(tunuino_dispatch_validate(&context)==TUNUINO_DISPATCH_OK);
    size_t n;const uint8_t *body=query((const uint8_t *)"S",1,&n);assert(body[0]==0&&strcmp((const char *)body+1,TUNUINO_IO_SIGNATURE)==0);vector("signature");
    /* Negotiation must advertise the CRC-wrapped protocol, not legacy 001. */
    body=query((const uint8_t *)"F",1,&n);assert(n==4&&body[0]==0&&memcmp(body+1,"002",3)==0);vector("ts_protocol");
    body=query((const uint8_t *)"C",1,&n);assert(n==2&&body[0]==0&&body[1]==0xFF);vector("ts_test");
    body=query((const uint8_t *)"c",1,&n);assert(n==3&&body[0]==0&&body[1]==0&&body[2]==0);vector("ts_seconds");
    body=query((const uint8_t *)"I",1,&n);assert(n==2&&body[0]==0&&body[1]==0);vector("ts_can_id");
    const uint8_t capability_command[]={'f',0};
    body=query(capability_command,sizeof(capability_command),&n);
    assert(n==6&&body[0]==0&&body[1]==2&&body[2]==0&&body[3]==64&&body[4]==0&&body[5]==64);
    vector("ts_capabilities");
    uint8_t plain[8];size_t plain_len=0;
    assert(tunuino_dispatch_plain_reply(&context,'F',plain,sizeof(plain),&plain_len)==TUNUINO_DISPATCH_OK);
    assert(plain_len==3&&memcmp(plain,"002",3)==0);
    assert(tunuino_dispatch_plain_reply(&context,'C',plain,sizeof(plain),&plain_len)==TUNUINO_DISPATCH_OK);
    assert(plain_len==2&&plain[0]==0&&plain[1]==0xFF);
    assert(tunuino_dispatch_plain_reply(&context,'I',plain,sizeof(plain),&plain_len)==TUNUINO_DISPATCH_OK);
    assert(plain_len==1&&plain[0]==0);
    body=query((const uint8_t *)"A",1,&n);assert(n==101&&body[0]==0);vector("runtime_default");
    uint8_t settings[576];memcpy(settings,device.active,576);
    settings[2]=1;settings[3]=60;settings[4]=2;settings[6]=1;settings[7]=2;settings[8]=1;
    put16(settings+10,1000);put16(settings+12,3000);put16(settings+14,0);put16(settings+16,20000);
    put16(settings+18,1000);put16(settings+20,3000);put16(settings+22,500);put16(settings+24,1500);
    settings[26]=3;put16(settings+32,1000);put16(settings+34,2000);put16(settings+36,3000);
    put16(settings+64,10000);put16(settings+66,5000);put16(settings+68,0);
    put16(settings+256+32,10000);put16(settings+416+32,65036); /* -5 degrees */
    assert(tunuino_device_validate(settings,sizeof(settings)));
    write_page(1,settings,256);write_page(2,settings+256,160);write_page(3,settings+416,160);
    assert(device.dirty&&device.active[6]==0);burn(0);assert(device.persisted&&!device.dirty&&device.active[6]==1);
    tunuino_map map;assert(tunuino_map_decode(device.ve.bytes,device.ve.length,&map)==TUNUINO_MAP_OK&&map.cells[0][0]==100000);
    assert(tunuino_map_decode(device.ignition.bytes,device.ignition.length,&map)==TUNUINO_MAP_OK&&map.cells[0][0]==-5000);
    uint16_t raw[16];for(unsigned i=0;i<16;++i)raw[i]=2000;
    tunuino_device_sample(&device,1234567,raw,true,1234500,true,12,3);
    assert(device.live.valid_mask==15&&device.live.rpm_milli==1234500&&device.live.map_milli_kpa==100000
        &&device.live.lambda_million==1000000&&device.live.coolant_milli_c==50000);
    body=query((const uint8_t *)"A",1,&n);assert(n==101&&body[39]==15);vector("runtime_configured");
    body=query((const uint8_t *)"TUNUINO:P",9,&n);assert(n==28&&body[1]==2&&body[2]==15);vector("partial_all");
    tunuino_device_sample(&device,1234568,raw,true,0,false,0,3);
    body=query((const uint8_t *)"TUNUINO:P",9,&n);assert(body[2]==14);vector("partial_no_rpm");
    body=query((const uint8_t *)"TUNUINO:L",9,&n);assert(n==1&&body[0]==1); /* Legacy NOT_READY unchanged. */
    raw[0]=4095;tunuino_device_sample(&device,1234569,raw,true,0,false,0,3);assert(device.live.valid_mask==12);
    tunuino_device_sample(&device,1234570,NULL,true,0,false,0,3);assert(device.live.valid_mask==0&&!device.adc_valid);
    memcpy(settings,device.active,576);settings[6]=12;write_page(1,settings,256);burn(0x84);assert(device.active[6]==1);
    /* Malformed writes/burns do not change active settings or terminate USB. */
    const uint8_t malformed[]={'w',0,1,0,255,0,2,0,0};body=query(malformed,sizeof(malformed),&n);assert(n==1&&body[0]==0x83);
    body=query((const uint8_t *)"A",1,&n);assert(n==101);vector("runtime_invalid");
    memcpy(device.staged,device.active,576);device.dirty=false;
    /* A blocking flash operation gets a fresh TX deadline, not a pre-erase one. */
    tunuino_link link;clock_ms=0;assert(tunuino_link_init(&link,&context,1000,1000,0)==TUNUINO_LINK_RECEIVING);
    uint8_t packet[16];size_t size,used;const uint8_t command[]={'b',0,1};
    assert(tunuino_frame_encode(command,3,packet,sizeof(packet),&size)==TUNUINO_FRAME_OK);
    assert(tunuino_link_feed(&link,packet,size,0,&used)==TUNUINO_LINK_RESPONDING&&clock_ms==2000);
    assert(tunuino_link_poll(&link,2000)==TUNUINO_LINK_RESPONDING);
    persistence();
    puts("OK");return 0;
}
