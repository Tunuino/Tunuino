/* Copyright (c) 2026 Tunuino. Legacy 1408-byte records migrate transactionally. */
#include "tunuino/config_journal.h"
#include "aux_fixture.h"
#include <assert.h>
#include <stdio.h>
static uint8_t flash[2][TUNUINO_JOURNAL_BANK_BYTES],baseline[2][TUNUINO_JOURNAL_BANK_BYTES];
static int remaining=-1;static bool erase_fails;
static bool read_bank(void *u,unsigned b,size_t o,uint8_t *p,size_t n){(void)u;assert(b<2&&o+n<=TUNUINO_JOURNAL_BANK_BYTES);memcpy(p,flash[b]+o,n);return true;}
static bool write_bank(void *u,unsigned b,size_t o,const uint8_t *p,size_t n){(void)u;assert(b<2&&o%4==0&&n%4==0);for(size_t i=0;i<n;i+=4){if(remaining==0)return false;if(remaining>0)--remaining;for(unsigned j=0;j<4;j++){assert(flash[b][o+i+j]==255);flash[b][o+i+j]=p[i+j];}}return true;}
static bool erase_bank(void *u,unsigned b){(void)u;if(erase_fails)return false;memset(flash[b],255,TUNUINO_JOURNAL_BANK_BYTES);return true;}
static void p32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void legacy_record(size_t offset,const uint8_t *settings,uint32_t sequence){
    uint8_t record[1440];memset(record,255,sizeof(record));memcpy(record,"TNUIO002",8);p32(record+8,sequence);p32(record+12,1408);memcpy(record+16,settings,1408);p32(record+1424,tunuino_crc32(record,1424));memcpy(record+1436,"DONE",4);memcpy(flash[0]+offset,record,sizeof(record));
}
int main(void){
    static tunuino_device d;tunuino_device_init(&d,NULL,NULL,NULL);memset(flash,255,sizeof(flash));
    legacy_record(0,d.active,8);d.active[101]=0xda; /* reference 3290mV */legacy_record(1440,d.active,9);
    uint8_t desired[TUNUINO_IO_SETTINGS_SIZE],loaded[TUNUINO_IO_SETTINGS_SIZE];memcpy(desired,d.active,sizeof(desired));aux_fixture(desired+1408);
    const tunuino_journal_io io={read_bank,write_bank,erase_bank,NULL};tunuino_config_journal j;
    assert(tunuino_journal_init(&j,&io,loaded)&&j.legacy&&j.sequence==9&&j.bank==0);
    assert(memcmp(loaded,d.active,1408)==0);for(unsigned i=1408;i<1664;++i)assert(!loaded[i]);
    assert(tunuino_device_restore(&d,loaded,1408)&&!memcmp(d.active,loaded,1664));
    memcpy(baseline,flash,sizeof(flash));
    for(unsigned cut=0;cut<=TUNUINO_JOURNAL_RECORD_BYTES/4;++cut){
        memcpy(flash,baseline,sizeof(flash));remaining=-1;assert(tunuino_journal_init(&j,&io,loaded));
        remaining=(int)cut;bool saved=tunuino_journal_save(&j,desired);remaining=-1;
        assert(tunuino_journal_init(&j,&io,loaded));assert(j.sequence==(saved?10u:9u));assert(j.legacy!=saved);
        assert(memcmp(loaded,saved?desired:d.active,sizeof(loaded))==0);
        assert(memcmp(flash[0],baseline[0],TUNUINO_JOURNAL_BANK_BYTES)==0); /* old bank untouched */
    }
    memcpy(flash,baseline,sizeof(flash));assert(tunuino_journal_init(&j,&io,loaded));erase_fails=true;
    assert(!tunuino_journal_save(&j,desired));erase_fails=false;assert(tunuino_journal_init(&j,&io,loaded)&&j.legacy);
    assert(tunuino_journal_save(&j,desired)&&!j.legacy&&j.bank==1);
    flash[j.bank][j.offset+17]^=1;assert(tunuino_journal_init(&j,&io,loaded)&&j.legacy&&j.sequence==9);
    puts("Journal migration: legacy profile/maps preserved, every programmed-word interruption, failed erase, committed new format and corrupt-new fallback passed.");return 0;
}
