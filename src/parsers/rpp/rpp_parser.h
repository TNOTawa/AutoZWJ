#pragma once
#include "core/project_model.h"
#include <cstdint>
#include <string>
#include <vector>

struct EndWarnings {
    std::vector<std::string> exist_mode2;
    std::vector<std::string> exist_stretch_marker;
};

struct RppXmidiEvent {
    uint64_t tick = 0;
    uint8_t status = 0;
    uint8_t data1 = 0;
    uint8_t data2 = 0;
};

struct RppXmidiSource {
    bool present = false;
    uint32_t ppq = 960;
    bool ignore_tempo = false;
    double tempo_bpm = 120.0;
    int tempo_beat = 4;
    std::vector<RppXmidiEvent> events;
};

struct RppXmidiItem {
    size_t object_index = 0;
    double item_position_sec = 0.0;
    double source_offset_sec = 0.0;
    RppXmidiSource source;
};

struct RppParseMetadata {
    double start_pos_sec = 0.0;
    std::vector<RppXmidiItem> xmidi_items;
};

bool parse_rpp(const std::string& path, ObjDict& objdict,
               std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths,
               EndWarnings& warnings,
               double start_pos = 0.0, double end_pos = 100000.0,
               const std::vector<int>* sel_tracks = nullptr,
               RppParseMetadata* metadata = nullptr);

bool parse_rpp_auto(const std::string& path, ObjDict& objdict,
                    std::vector<TrackNode>& tracks, std::vector<std::string>& file_paths,
                    EndWarnings& warnings,
                    double start_pos = 0.0, double end_pos = 100000.0,
                    RppParseMetadata* metadata = nullptr);

void expand_rpp_xmidi_items(ObjDict& objdict, std::vector<TrackNode>& tracks,
                            const RppParseMetadata& metadata);
