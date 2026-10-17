// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "firmware_image.h"
static int nibble(uint8_t c) { if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1; }
static int byte_at(const uint8_t *p, size_t at) { int a=nibble(p[at]),b=nibble(p[at+1u]);return (a<0||b<0)?-1:(a<<4)|b; }
uint8_t tunuino_firmware_intel_hex_check(const uint8_t *data,size_t length) {
    if(!data||!length||length>16u*1024u*1024u)return 0u;
    size_t i=0u;uint8_t eof=0u,records=0u;
    while(i<length) {
        if(data[i]!=':'||eof)return 0u;
        size_t start=i++;
        while(i<length&&data[i]!='\n'&&data[i]!='\r')i++;
        size_t n=i-start-1u;
        if(n<10u||(n&1u))return 0u;
        int count=byte_at(data,start+1u);
        if(count<0||n!=(size_t)(count+5)*2u)return 0u;
        int addr_hi=byte_at(data,start+3u),addr_lo=byte_at(data,start+5u),type=byte_at(data,start+7u);
        if(addr_hi<0||addr_lo<0||type<0)return 0u;
        unsigned sum=(unsigned)count+(unsigned)addr_hi+(unsigned)addr_lo+(unsigned)type;
        for(size_t j=0;j<(size_t)count+1u;j++){int v=byte_at(data,start+9u+j*2u);if(v<0)return 0u;sum+=(unsigned)v;}
        if((sum&255u)!=0u)return 0u;
        if(type==1){if(count!=0||addr_hi||addr_lo)return 0u;eof=1u;}
        else if(type==4||type==2){if(count!=2)return 0u;}
        else if(type==3||type==5){if(count!=4)return 0u;}
        else if(type!=0)return 0u;
        records=1u;
        if(i<length&&data[i]=='\r')i++;
        if(i<length&&data[i]=='\n')i++;
    }
    return (uint8_t)(records&&eof);
}
tunuino_fw_format_t tunuino_firmware_detect(const uint8_t *data,size_t length){
    if(!data||!length)return TUNUINO_FW_UNKNOWN;
    if(data[0]==':')return tunuino_firmware_intel_hex_check(data,length)?TUNUINO_FW_HEX:TUNUINO_FW_UNKNOWN;
    return TUNUINO_FW_BIN;
}
