#pragma once
#include "plugin.h"
#include <string>
#include <vector>

struct GenerateContext {
    std::string alias_data;
    int layer;
    int start_frame;
    int length;
    std::wstring name;
    std::string serialized_config;
};

int generate_objects(const ObjDict& objdict,
                     const std::vector<TrackNode>& tracks,
                     const OutputConfig& config,
                     const std::vector<std::string>& file_paths,
                     std::vector<GenerateContext>& out_contexts);

double seconds_to_frames(double seconds, int fps_num, int fps_den);

enum FlipType {
    FLIP_NONE = 0,
    FLIP_HORIZONTAL = 1,
    FLIP_VERTICAL = 2,
    FLIP_CW = 3,
    FLIP_CCW = 4,
};
