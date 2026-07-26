#pragma once
#include <vector>
#include <string>
#include "core/output_config.h"

struct TempoPoint {
    double time_sec = 0.0;
    double bpm = 120.0;
    int beat = 4;
};

struct ObjDict {
    std::vector<double> pos;
    std::vector<double> length;
    std::vector<int>    loop;
    std::vector<double> soffs;
    std::vector<double> pitch;
    std::vector<double> playrate;
    std::vector<int>    fileidx;
    std::vector<std::string> filetype;
    std::vector<std::string> filelist;
    std::vector<int>    midi_note;
    std::vector<std::string> midi_lyric;
    std::vector<double> midi_len;
    std::vector<double> midi_volume;     // CC7, 0-127, -1 = 未设置
    std::vector<double> midi_pan;        // CC10, 0-127 (64=center), -1 = 未设置
    std::vector<double> midi_pitch_bend; // -8192~8191, 0 = 中心
    double bpm = 120.0;
    std::vector<TempoPoint> tempo_map;
    int track_count = 0;
};

struct FileHistoryEntry {
    std::wstring path;
    double base_time_sec = 0.0;
};

struct TrackNode {
    std::wstring name;
    int number = 0;
    int index = 0;
    int count = 0;
    bool is_bus = false;
    int depth = 0;
    bool solo = false;
    bool mute = false;
    bool selected = true;
    std::vector<TrackNode> children;
};

struct ParsedProject {
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    std::vector<std::string> file_paths;
};

struct ProjectState {
    bool has_data = false;
    std::wstring file_path;
    std::wstring file_name;
    std::wstring last_directory;
    ObjDict objdict;
    std::vector<TrackNode> tracks;
    OutputConfig config;
    std::string template_alias;
    int template_layer = 0;
    std::vector<FileHistoryEntry> file_history;
};