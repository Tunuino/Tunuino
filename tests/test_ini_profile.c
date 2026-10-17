// SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
#include "../firmware/console/ini_profile.h"
#include <assert.h>
#include <string.h>
int main(void) {
    const char *speed="[MegaTune]\nqueryCommand = \"Q\"\nsignature = \"speeduino 202504-dev\"\n[TunerStudio]\niniSpecVersion=3.64\n";
    const char *other="[MegaTune]\nsignature = \"Tunuino-STM32F429\" ; comment\n";
    tunuino_ini_profile_t p;
    assert(tunuino_ini_profile_parse(speed,strlen(speed),&p));
    assert(p.query_found && strcmp(p.query_command,"Q")==0);
    assert(tunuino_ini_signature_matches(&p,"speeduino 202504-dev"));
    assert(!tunuino_ini_signature_matches(&p,"Tunuino-STM32F429"));
    const char *utf8_bom="\xEF\xBB\xBF[MegaTune]\nsignature = \"Tunuino-STM32F429\"\n";
    assert(tunuino_ini_profile_parse(utf8_bom,strlen(utf8_bom),&p));
    assert(tunuino_ini_signature_matches(&p,"Tunuino-STM32F429"));
    assert(tunuino_ini_profile_parse(other,strlen(other),&p));
    assert(tunuino_ini_signature_matches(&p,"Tunuino-STM32F429"));
    const char *duplicate_signature="[MegaTune]\nsignature=\"ECU-A\"\nsignature=\"ECU-B\"\n";
    const char *duplicate_query="[MegaTune]\nsignature=\"ECU-A\"\nqueryCommand=\"Q\"\nqueryCommand=\"S\"\n";
    assert(!tunuino_ini_profile_parse(duplicate_signature,strlen(duplicate_signature),&p));
    assert(!tunuino_ini_profile_parse(duplicate_query,strlen(duplicate_query),&p));
    assert(!tunuino_ini_profile_parse("[MegaTune]\nsignature = \"unterminated",35,&p));
    return 0;
}
