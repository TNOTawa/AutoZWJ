#pragma once
#include "plugin.h"
#include <string>
#include <vector>

bool parse_midi(const std::string& path, ObjDict& objdict,
                std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths);
