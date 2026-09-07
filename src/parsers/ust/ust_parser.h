#pragma once

#include "core/project_model.h"

#include <string>
#include <vector>

bool parse_ust(const std::string& path, ObjDict& objdict,
               std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths);
