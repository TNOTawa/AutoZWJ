#pragma once
#include <string>
#include <vector>
#include "core/effect_model.h"

struct TemplateEntry {
    int object_id = 0;
    std::string alias;
    std::string display_name;
    int layer = 0;
    double sf = 0.0;
    int ef = 0;
    std::string chain;
};

struct SceneInfo {
    int width = 1920;
    int height = 1080;
    int rate = 60;
    int scale = 1;
    int sample_rate = 44100;
    int frame = 0;
    int layer = 0;
    bool valid = false;
};

enum class ObjNameKind : int { FilePath = 0, Gap = 1, Item = 2 };

struct GeneratedObject {
    int layer;
    int sf;
    int ef;
    std::string alias_chain;
    ObjNameKind name_kind = ObjNameKind::Item;
    std::string name_text;
    int name_index = 0;
};

struct TemplateSession {
    TemplateEntry source;
    std::vector<ParsedEffect> effects;
    std::vector<ParamBake> bakes;
    std::vector<PresetEntry> presets;
};