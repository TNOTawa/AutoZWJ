#pragma once
#include "plugin.h"
#include <string>
#include <vector>

struct EndWarnings {
    std::vector<std::string> exist_mode2;
    std::vector<std::string> exist_stretch_marker;
};

bool parse_rpp(const std::string& path, ObjDict& objdict,
               std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths,
               EndWarnings& warnings,
               double start_pos = 0.0, double end_pos = 100000.0,
               const std::vector<int>* sel_tracks = nullptr);

bool parse_rpp_auto(const std::string& path, ObjDict& objdict,
                    std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths,
                    EndWarnings& warnings,
                    double start_pos = 0.0, double end_pos = 100000.0);
