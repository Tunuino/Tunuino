// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "ini_profile.h"
#include <string.h>
static uint8_t whitespace(char c) { return (uint8_t)(c==' ' || c=='\t' || c=='\r'); }
static uint8_t extract(const char *value, size_t n, char *out, size_t cap) {
    size_t i=0u, k=0u;
    while (i<n && whitespace(value[i])) i++;
    if (i<n && value[i]=='"') {
        i++;
        while (i<n && value[i]!='"') {
            if (k+1u>=cap) return 0u;
            out[k++]=value[i++];
        }
        if (i==n || value[i]!='"') return 0u;
        i++;
    } else {
        while (i<n && value[i]!=';' && !whitespace(value[i])) {
            if (k+1u>=cap) return 0u;
            out[k++]=value[i++];
        }
    }
    if (k==0u) return 0u;
    while (i<n && whitespace(value[i])) i++;
    if (i<n && value[i]!=';') return 0u;
    out[k]='\0';
    return 1u;
}
uint8_t tunuino_ini_profile_parse(const char *data, size_t length, tunuino_ini_profile_t *out) {
    if (!data || !out || length>1024u*1024u) return 0u;
    memset(out,0,sizeof(*out));
    uint8_t in_megatune=0u;
    size_t pos=0u;
    while (pos<length) {
        size_t end=pos;
        while (end<length && data[end]!='\n') end++;
        size_t start=pos;
        while (start<end && whitespace(data[start])) start++;
        size_t stop=end;
        while (stop>start && whitespace(data[stop-1u])) stop--;
        if (start<stop && data[start]=='[') {
            in_megatune=(uint8_t)(stop-start==10u && memcmp(data+start,"[MegaTune]",10u)==0);
        } else if (in_megatune && start<stop && data[start]!=';' && data[start]!='#') {
            size_t eq=start;
            while (eq<stop && data[eq]!='=') eq++;
            if (eq<stop) {
                size_t keyend=eq;
                while (keyend>start && whitespace(data[keyend-1u])) keyend--;
                if (keyend-start==9u && memcmp(data+start,"signature",9u)==0) {
                    if (!extract(data+eq+1u,stop-eq-1u,out->signature,sizeof(out->signature))) return 0u;
                    out->signature_found=1u;
                } else if (keyend-start==12u && memcmp(data+start,"queryCommand",12u)==0) {
                    if (!extract(data+eq+1u,stop-eq-1u,out->query_command,sizeof(out->query_command))) return 0u;
                    out->query_found=1u;
                }
            }
        }
        pos=end<length?end+1u:end;
    }
    return out->signature_found;
}
uint8_t tunuino_ini_signature_matches(const tunuino_ini_profile_t *profile, const char *ecu_signature) {
    return (uint8_t)(profile && ecu_signature && profile->signature_found && strcmp(profile->signature,ecu_signature)==0);
}
