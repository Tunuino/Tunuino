/* Copyright (c) 2026 Tunuino. Desktop protocol simulator; no hardware access. */
#include "model.h"
#include "tunuino/link.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
static sim_model model;
static tunuino_link link;

static int usage(void) {
    fprintf(stderr,"Usage: tunuino-simulator --stdio [--family proteus|uaefi] [--rpm 600..10000]\n"
        " [--scenario normal|stall|sensor-loss|wrap] [--bench] [--ve map.tnmap] [--ignition map.tnmap]\n");
    return 2;
}

int main(int argc, char **argv) {
    tunuino_family family = TUNUINO_PROTEUS;
    sim_scenario scenario = SIM_NORMAL;
    unsigned rpm = 1000;
    int stdio = 0, bench = 0;
    const char *ve = NULL, *ignition = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i],"--help") == 0) { (void)usage(); return 0; }
        if (strcmp(argv[i],"--stdio") == 0) { stdio = 1; continue; }
        if (strcmp(argv[i],"--bench") == 0) { bench = 1; continue; }
        if (i + 1 >= argc) return usage();
        const char *key = argv[i], *value = argv[++i];
        if (strcmp(key,"--family") == 0) {
            if (strcmp(value,"proteus") == 0) family = TUNUINO_PROTEUS;
            else if (strcmp(value,"uaefi") == 0) family = TUNUINO_UAEFI;
            else return usage();
        } else if (strcmp(key,"--scenario") == 0) {
            if (strcmp(value,"normal") == 0) scenario = SIM_NORMAL;
            else if (strcmp(value,"stall") == 0) scenario = SIM_STALL;
            else if (strcmp(value,"sensor-loss") == 0) scenario = SIM_SENSOR_LOSS;
            else if (strcmp(value,"wrap") == 0) scenario = SIM_WRAP;
            else return usage();
        } else if (strcmp(key,"--rpm") == 0) {
            char *end;
            unsigned long parsed = strtoul(value,&end,10);
            if (*value == '\0' || *end != '\0' || parsed < 600 || parsed > 10000) return usage();
            rpm = (unsigned)parsed;
        } else if (strcmp(key,"--ve") == 0) ve = value;
        else if (strcmp(key,"--ignition") == 0) ignition = value;
        else return usage();
    }
    if (!stdio) return usage();
#ifdef _WIN32
    if (_setmode(_fileno(stdin),_O_BINARY) < 0 || _setmode(_fileno(stdout),_O_BINARY) < 0) return 3;
#endif
    if (!sim_model_init(&model,family,rpm,scenario,bench)
        || (ve != NULL && !sim_model_load_map(&model,ve,TUNUINO_MAP_VE))
        || (ignition != NULL && !sim_model_load_map(&model,ignition,TUNUINO_MAP_IGNITION))) {
        fputs("Invalid simulator configuration/map\n",stderr); return 4;
    }
    fprintf(stderr,"Tunuino SIMULATOR: synthetic identity, captures, sensors and maps; no ECU connection.\n");
    tunuino_dispatch_context context = {&model.identity,&model.live,&model.ve,&model.ignition,NULL};
    if (tunuino_link_init(&link,&context,1000,1000,model.now_us / 1000u) != TUNUINO_LINK_RECEIVING) return 5;
    int byte;
    while ((byte = getchar()) != EOF) {
        uint8_t input = (uint8_t)byte;
        size_t consumed;
        tunuino_link_status status = tunuino_link_feed(&link,&input,1,model.now_us / 1000u,&consumed);
        if (consumed != 1 || (status != TUNUINO_LINK_RECEIVING && status != TUNUINO_LINK_RESPONDING)) {
            fputs("Invalid or unsupported simulator request\n",stderr); return 6;
        }
        if (status != TUNUINO_LINK_RESPONDING) continue;
        const uint8_t *output;
        size_t count;
        if (tunuino_link_tx_view(&link,model.now_us / 1000u,&output,&count) != TUNUINO_LINK_RESPONDING) return 7;
        /* Only live requests advance virtual acquisition time; map snapshots stay immutable. */
        int live = !link.plain_response && ((count == 34 && output[3] == 1 && output[4] == 15)
            || (count == 7 && output[2] == 1));
        if (fwrite(output,1,count,stdout) != count || fflush(stdout) != 0
            || tunuino_link_tx_advance(&link,count,model.now_us / 1000u) != TUNUINO_LINK_RECEIVING) return 8;
        if (live && !sim_model_step(&model)) return 9;
    }
    tunuino_link_abort(&link);
    return ferror(stdin) ? 10 : 0;
}
