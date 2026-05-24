#pragma once
#include "plugin.h"
#include <vector>
#include <map>
#include <string>

const std::vector<EffectDef>& get_effect_registry();

const EffectDef* find_effect(const std::string& name);

const std::map<int, std::string>& get_blend_modes_ja();
const std::map<int, std::string>& get_blend_modes_en();

struct MediaInputEffect {
    std::wstring name;
    std::string key;
    std::string file_param;
};

const std::vector<MediaInputEffect>& get_media_input_effects();
