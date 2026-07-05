#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "plugin.h"
#include "ui/effect_chain_editor.h"

struct GenInput {
    ObjDict* objdict;
    std::vector<TrackNode>* tracks;
    OutputConfig* config;
    std::vector<TemplateEntry> template_pool;
    std::vector<std::vector<ParamBake>>* bakes_per_tpl;
    std::vector<std::vector<PresetEntry>>* presets_per_tpl;
    SceneInfo scene;
    int base_layer;
    uint32_t seed_base;
    std::vector<int> shuffled_order;
    LOG_HANDLE* logger;
};

struct GeneratedObject {
    int layer;
    int sf;
    int ef;
    std::string alias_chain;
    std::wstring name;
};

std::vector<GeneratedObject> generate_object_specs(const GenInput& in);
