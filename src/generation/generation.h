#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <span>
#include "core/project_model.h"
#include "core/generation_model.h"
#include "core/effect_model.h"
#include "core/output_config.h"

struct GenerationInput {
    const ObjDict& objdict;
    const std::vector<TrackNode>& tracks;
    const OutputConfig& config;
    std::span<const TemplateSession> templates;
    SceneInfo scene;
    int base_layer;
    uint32_t seed;
};

struct GenerationResult {
    std::vector<GeneratedObject> objects;
    std::vector<std::string> warnings;
};

GenerationResult generate(const GenerationInput& in);