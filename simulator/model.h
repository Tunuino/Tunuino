/* Copyright (c) 2026 Tunuino. Desktop simulation only. */
#ifndef TUNUINO_SIMULATOR_MODEL_H
#define TUNUINO_SIMULATOR_MODEL_H
#include "tunuino/capture_source.h"
#include "tunuino/dispatch.h"

typedef enum { SIM_NORMAL, SIM_STALL, SIM_SENSOR_LOSS, SIM_WRAP } sim_scenario;
typedef struct {
    tunuino_identity identity;
    tunuino_live_sample live;
    tunuino_map_snapshot ve, ignition;
    tunuino_capture_source source;
    uint64_t now_us, last_edge_us;
    uint32_t interval_us;
    unsigned tooth, samples, was_stalled;
    sim_scenario scenario;
} sim_model;
int sim_model_init(sim_model *model, tunuino_family family, unsigned rpm, sim_scenario scenario, int bench);
int sim_model_step(sim_model *model);
int sim_model_load_map(sim_model *model, const char *path, tunuino_map_kind kind);
#endif
