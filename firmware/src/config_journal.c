/* Copyright (c) 2026 Tunuino. Two flash sectors, append records, commit marker last. */
#include "tunuino/config_journal.h"
#include <string.h>

static const uint8_t magic[8]={'T','N','U','I','O','0','0','1'};
static const uint8_t marker[4]={'D','O','N','E'};
static uint32_t get32(const uint8_t *p) {return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void put32(uint8_t *p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static bool erased(const uint8_t *p,size_t size){for(size_t i=0;i<size;++i)if(p[i]!=255)return false;return true;}
static bool valid(const uint8_t record[608]) {
    return memcmp(record,magic,8)==0 && get32(record+12)==576
        && memcmp(record+604,marker,4)==0 && get32(record+592)==tunuino_crc32(record,592)
        && erased(record+596,8) && tunuino_device_validate(record+16,576);
}
bool tunuino_journal_init(tunuino_config_journal *j,const tunuino_journal_io *io,uint8_t settings[576]) {
    if(j==NULL||io==NULL||settings==NULL||io->read==NULL||io->program==NULL||io->erase==NULL)return false;
    memset(j,0,sizeof(*j));j->io=*io;uint8_t record[608];
    for(unsigned bank=0;bank<2;++bank) for(size_t offset=0;offset+608<=TUNUINO_JOURNAL_BANK_BYTES;offset+=608) {
        if(!io->read(io->user,bank,offset,record,sizeof(record)))return false;
        if(valid(record)) {
            uint32_t sequence=get32(record+8);
            if(!j->found || (uint32_t)(sequence-j->sequence)<UINT32_C(0x80000000)) {
                j->found=true;j->sequence=sequence;j->bank=bank;j->offset=offset;memcpy(settings,record+16,576);
            }
        }
    }
    return j->found;
}
bool tunuino_journal_save(tunuino_config_journal *j,const uint8_t settings[576]) {
    if(j==NULL||!tunuino_device_validate(settings,576))return false;
    uint8_t record[608];unsigned bank=j->found?j->bank:0;size_t offset=j->found?j->offset+608:0;
    if(j->found) {
        if(!j->io.read(j->io.user,j->bank,j->offset,record,sizeof(record)))return false;
        if(valid(record)&&memcmp(record+16,settings,576)==0)return true;
    }
    /* Torn records consume their slot. Never program over a non-erased word. */
    for(;offset+608<=TUNUINO_JOURNAL_BANK_BYTES;offset+=608) {
        if(!j->io.read(j->io.user,bank,offset,record,sizeof(record)))return false;
        if(erased(record,sizeof(record)))break;
    }
    if(offset+608>TUNUINO_JOURNAL_BANK_BYTES) {
        bank=j->found?1u-j->bank:0;
        if(!j->io.erase(j->io.user,bank))return false;
        offset=0;
    }
    memset(record,255,sizeof(record));memcpy(record,magic,8);
    uint32_t sequence=j->found?j->sequence+1:1;
    put32(record+8,sequence);put32(record+12,576);memcpy(record+16,settings,576);
    put32(record+592,tunuino_crc32(record,592));
    if(!j->io.program(j->io.user,bank,offset,record,604))return false;
    if(!j->io.program(j->io.user,bank,offset+604,marker,4))return false;
    if(!j->io.read(j->io.user,bank,offset,record,sizeof(record))||!valid(record)
        ||memcmp(record+16,settings,576)!=0)return false;
    j->found=true;j->sequence=sequence;j->bank=bank;j->offset=offset;return true;
}
