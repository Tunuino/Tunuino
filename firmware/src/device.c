/* Copyright (c) 2026 Tunuino. Input acquisition and calibration bench, no actuators. */
#include "tunuino/device.h"
#include <string.h>

uint16_t tunuino_device_u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] << 8 | p[1]); }
static void put16(uint8_t *p, uint16_t value) { p[0]=(uint8_t)(value>>8);p[1]=(uint8_t)value; }
static void put32(uint8_t *p, uint32_t value) { put16(p,(uint16_t)(value>>16));put16(p+2,(uint16_t)value); }
static int32_t signed16(const uint8_t *p) {
    uint16_t value=tunuino_device_u16(p);
    return value < 32768 ? (int32_t)value : (int32_t)value-65536;
}
static size_t page_offset(unsigned page) { return page==1 ? 0 : page==2 ? 256 : 416; }
static size_t page_size(unsigned page) { return page==1 ? 256 : 160; }

static bool map_decode(tunuino_map *map, const uint8_t *bytes, tunuino_map_kind kind) {
    memset(map,0,sizeof(*map)); map->kind=kind;map->rpm_count=map->load_count=8;
    for (unsigned i=0;i<8;++i) { map->rpm[i]=tunuino_device_u16(bytes+2*i);map->load_milli_kpa[i]=(int32_t)tunuino_device_u16(bytes+16+2*i)*10; }
    for (unsigned r=0;r<8;++r) for (unsigned c=0;c<8;++c)
        map->cells[r][c]=signed16(bytes+32+2*(r*8+c))*10;
    return tunuino_map_validate(map)==TUNUINO_MAP_OK;
}

bool tunuino_device_validate(const uint8_t *p, size_t length) {
    if (p==NULL || length!=TUNUINO_IO_SETTINGS_SIZE || tunuino_device_u16(p)!=1
        || p[2]>1 || p[3]<4 || p[3]>120 || p[4]<1 || p[4]>=p[3]/2
        || p[5]!=0 || p[6]>11 || p[7]>11 || p[8]>4 || p[9]!=0
        || tunuino_device_u16(p+10)>=tunuino_device_u16(p+12) || tunuino_device_u16(p+12)>4095
        || (p[6] && tunuino_device_u16(p+14)>=tunuino_device_u16(p+16)) || tunuino_device_u16(p+16)>50000
        || tunuino_device_u16(p+18)>=tunuino_device_u16(p+20) || tunuino_device_u16(p+20)>4095
        || (p[7] && (tunuino_device_u16(p+22)==0 || tunuino_device_u16(p+22)>=tunuino_device_u16(p+24)))
        || tunuino_device_u16(p+24)>5000 || p[26]!=3
        || tunuino_device_u16(p+96)<1000 || tunuino_device_u16(p+96)>20000
        || tunuino_device_u16(p+100)<2800 || tunuino_device_u16(p+100)>3600) return false;
    for (unsigned i=27;i<32;++i) if(p[i]!=0) return false;
    for (unsigned i=98;i<100;++i) if(p[i]!=0) return false;
    for (unsigned i=102;i<256;++i) if(p[i]!=0) return false;
    for (unsigned i=0;i<16;++i) {
        unsigned raw=tunuino_device_u16(p+32+2*i);int32_t value=signed16(p+64+2*i);
        if (i<p[26]) { if(raw>4095 || value < -4000 || value>15000 || (i && raw<=tunuino_device_u16(p+30+2*i))) return false; }
        else if(raw!=0 || value!=0) return false;
    }
    if(p[8] && signed16(p+64)==signed16(p+66) && signed16(p+66)==signed16(p+68))return false;
    /* Validate axes and ranges without placing two maximum-size maps on a stack. */
    for (unsigned page=2;page<=3;++page) {
        const uint8_t *m=p+page_offset(page);
        for (unsigned i=0;i<8;++i) {
            if (tunuino_device_u16(m+2*i)>20000 || tunuino_device_u16(m+16+2*i)>50000
                || (i && (tunuino_device_u16(m+2*i)<=tunuino_device_u16(m+2*i-2)
                    || tunuino_device_u16(m+16+2*i)<=tunuino_device_u16(m+14+2*i)))) return false;
        }
        for (unsigned i=0;i<64;++i) {
            int32_t value=signed16(m+32+2*i);
            if ((page==2 && (value<0 || value>30000)) || (page==3 && (value < -9000 || value>9000))) return false;
        }
    }
    return true;
}

static bool publish(tunuino_device *d) {
    if (++d->generation==0) d->generation=1;
    if (!map_decode(&d->scratch,d->active+256,TUNUINO_MAP_VE)
        || tunuino_map_snapshot_prepare(&d->ve,&d->scratch,d->generation)!=TUNUINO_MAP_OK
        || !map_decode(&d->scratch,d->active+416,TUNUINO_MAP_IGNITION)
        || tunuino_map_snapshot_prepare(&d->ignition,&d->scratch,d->generation)!=TUNUINO_MAP_OK) return false;
    return true;
}

void tunuino_device_init(tunuino_device *d, tunuino_device_save save, tunuino_device_clock clock, void *user) {
    memset(d,0,sizeof(*d));d->save=save;d->clock=clock;d->user=user;
    uint8_t *p=d->active;
    put16(p,1);p[3]=36;p[4]=1; /* Acquisition remains disabled until configured. */
    put16(p+10,0);put16(p+12,4095);
    put16(p+18,0);put16(p+20,4095);
    /* An explicit user calibration is required before selecting an input. */
    p[26]=3;put16(p+32,0);put16(p+34,2048);put16(p+36,4095);
    put16(p+96,12000);put16(p+100,3300);
    const uint16_t rpm[8]={500,1000,1500,2000,3000,4000,6000,8000};
    const uint16_t load[8]={2000,4000,6000,8000,10000,12000,16000,20000};
    for (unsigned page=2;page<=3;++page) for(unsigned i=0;i<8;++i) {
        put16(p+page_offset(page)+2*i,rpm[i]);put16(p+page_offset(page)+16+2*i,load[i]);
    }
    memcpy(d->staged,d->active,sizeof(d->active));(void)publish(d);
}

bool tunuino_device_restore(tunuino_device *d, const uint8_t *bytes, size_t length) {
    if(d==NULL || !tunuino_device_validate(bytes,length))return false;
    memmove(d->active,bytes,length);memcpy(d->staged,d->active,length);d->dirty=false;d->persisted=true;
    return publish(d);
}

void tunuino_device_sample(tunuino_device *d, uint64_t time_ms, const uint16_t raw[16],
    bool adc_valid, uint32_t rpm_milli, bool rpm_valid, uint16_t tooth, uint32_t errors) {
    tunuino_telemetry_sensors sensors={0};tunuino_analog_point points[16];
    const uint8_t *p=d->active;
    d->adc_valid=adc_valid&&raw!=NULL;
    d->trigger_valid=rpm_valid&&p[2]&&rpm_milli<=(uint32_t)tunuino_device_u16(p+96)*1000u;
    d->tooth=tooth;d->trigger_errors=errors;
    memset(&d->live,0,sizeof(d->live));d->live.time_ms=time_ms;
    if (adc_valid && raw!=NULL) {
        for(unsigned i=0;i<16;++i) if(raw[i]>4095) d->adc_valid=false;
        if(d->adc_valid)memcpy(d->raw,raw,sizeof(d->raw));
    }
    if(!d->adc_valid)memset(d->raw,0,sizeof(d->raw));
    if(rpm_valid && p[2] && rpm_milli <= (uint32_t)tunuino_device_u16(p+96)*1000u) {
        d->live.rpm_milli=rpm_milli;d->live.valid_mask|=TUNUINO_LIVE_RPM_VALID;
    }
    if(d->adc_valid && p[6]) {
        points[0]=(tunuino_analog_point){tunuino_device_u16(p+10),(int32_t)tunuino_device_u16(p+14)*10};
        points[1]=(tunuino_analog_point){tunuino_device_u16(p+12),(int32_t)tunuino_device_u16(p+16)*10};
        const tunuino_analog_curve curve={points,2,0,500000};
        (void)tunuino_analog_update(&sensors,TUNUINO_ANALOG_MAP,&curve,raw[p[6]-1],true,0);
    }
    if(d->adc_valid && p[7]) {
        points[0]=(tunuino_analog_point){tunuino_device_u16(p+18),(int32_t)tunuino_device_u16(p+22)*1000};
        points[1]=(tunuino_analog_point){tunuino_device_u16(p+20),(int32_t)tunuino_device_u16(p+24)*1000};
        const tunuino_analog_curve curve={points,2,1,5000000};
        (void)tunuino_analog_update(&sensors,TUNUINO_ANALOG_LAMBDA,&curve,raw[p[7]-1],true,0);
    }
    if(d->adc_valid && p[8]) {
        for(unsigned i=0;i<p[26];++i)points[i]=(tunuino_analog_point){tunuino_device_u16(p+32+2*i),signed16(p+64+2*i)*10};
        const tunuino_analog_curve curve={points,p[26],-40000,150000};
        (void)tunuino_analog_update(&sensors,TUNUINO_ANALOG_COOLANT,&curve,raw[11+p[8]],true,0);
    }
    d->live.valid_mask|=sensors.valid_mask;d->live.map_milli_kpa=sensors.map_milli_kpa;
    d->live.lambda_million=sensors.lambda_million;d->live.coolant_milli_c=sensors.coolant_milli_c;
}

static void runtime(tunuino_device *d, const tunuino_identity *id, uint8_t out[100]) {
    memset(out,0,100);uint8_t packet[38],query[14];size_t size;const uint8_t *body;
    (void)tunuino_frame_encode((const uint8_t *)"TUNUINO?",8,query,sizeof(query),&size);
    (void)tunuino_identity_reply(id,query,size,packet,sizeof(packet),&size);
    (void)tunuino_frame_decode(packet,size,&body,&size);memcpy(out,body+1,31);
    put32(out+32,(uint32_t)(d->live.time_ms/1000));put16(out+36,(uint16_t)(d->live.time_ms%1000));
    out[38]=d->live.valid_mask;out[39]=(uint8_t)((d->adc_valid?1:0)|(d->dirty?2:0)|(d->persisted?4:0)|(d->trigger_valid?8:0));
    put32(out+40,d->live.rpm_milli);put32(out+44,d->live.map_milli_kpa);put32(out+48,d->live.lambda_million);
    put32(out+52,(uint32_t)d->live.coolant_milli_c);put32(out+56,d->generation);
    for(unsigned i=0;i<16;++i)put16(out+60+2*i,d->raw[i]);
    put16(out+92,tunuino_device_u16(d->active+100));put16(out+94,d->tooth);put32(out+96,d->trigger_errors);
}

tunuino_dispatch_status tunuino_device_reply(tunuino_device *d,const tunuino_identity *id,
    const uint8_t *q,size_t length,uint8_t *response,size_t capacity,size_t *written) {
    uint8_t body[257]={0};size_t size=1;
    if(length==1 && q[0]=='D') {memcpy(d->staged,d->active,sizeof(d->staged));d->dirty=false;}
    else if(length==1 && q[0]=='A') {runtime(d,id,body+1);size=101;}
    else if(length==1 && (q[0]=='S'||q[0]=='Q'||q[0]=='V'||q[0]=='F')) {
        const char *text=q[0]=='V'?TUNUINO_IO_VERSION:q[0]=='F'?TUNUINO_IO_TS_SERIAL_VERSION:TUNUINO_IO_SIGNATURE;
        size_t count=q[0]=='F'?3:strlen(text)+1;memcpy(body+1,text,count);size=1+count;
    } else if(length==1 && (q[0]=='C'||q[0]=='c')) {
        /* TunerStudio communication checks, with the envelope status first. */
        if (q[0]=='C') { body[1]=0xFF;size=2; }
        else { put16(body+1,(uint16_t)(d->live.time_ms/1000u));size=3; }
    } else if(length==1 && q[0]=='I') {
        body[1]=0;size=2; /* The local ECU has CAN ID zero. */
    } else if(length==2 && q[0]=='f' && q[1]==0) {
        body[1]=2;put16(body+2,64);put16(body+4,64);size=6;
    } else if(length>=3 && (q[0]=='r'||q[0]=='w'||q[0]=='b'||q[0]=='k')) {
        unsigned page=tunuino_device_u16(q+1);
        if(page<1||page>3)body[0]=0x83;
        else if(q[0]=='b' && length==3) {
            if(!tunuino_device_validate(d->staged,sizeof(d->staged)))body[0]=0x84;
            else if(d->save==NULL || !d->save(d->user,d->staged,sizeof(d->staged)))body[0]=0x80;
            else if(!tunuino_device_restore(d,d->staged,sizeof(d->staged)))body[0]=0x80;
        } else if(q[0]=='k' && length==3) {
            put32(body+1,tunuino_crc32(d->staged+page_offset(page),page_size(page)));size=5;
        } else if(length>=7 && (q[0]=='r'||q[0]=='w')) {
            unsigned offset=tunuino_device_u16(q+3),count=tunuino_device_u16(q+5);
            if(count==0||count>256||offset>page_size(page)||count>page_size(page)-offset
                ||length!=(q[0]=='r'?7u:7u+count))body[0]=0x83;
            else if(q[0]=='r'){memcpy(body+1,d->staged+page_offset(page)+offset,count);size=1+count;}
            else {memcpy(d->staged+page_offset(page)+offset,q+7,count);d->dirty=memcmp(d->active,d->staged,sizeof(d->active))!=0;}
        } else body[0]=0x83;
    } else body[0]=0x83;
    return tunuino_frame_encode(body,size,response,capacity,written)==TUNUINO_FRAME_OK?TUNUINO_DISPATCH_OK:TUNUINO_DISPATCH_CAPACITY;
}
