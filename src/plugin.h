#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include "plugin2.h"
#include "logger2.h"

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
    double bpm = 120.0;
    int track_count = 0;
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

struct OutputConfig {
    int fps_num = 60;
    int fps_den = 1;
    int output_type = 0;
    bool alt_flip = false;
    int flip_type = 0;
    bool even_only = false;
    bool adjustment = false;
    bool beatless_sync = false;
    bool no_gap = true;
    bool use_round_up = false;
    int clipping = 0;
    int is_ex_set = 0;
    bool random_play = false;
    double random_end = 0.0;
    bool play_pos_by_frame = false;
    std::string src_path;
    std::string exedit_lang = "ja";
    std::vector<std::string> selected_effects;
    std::map<std::string, std::map<std::string, std::string>> effect_overrides;
};

struct EffectParam {
    std::string name;
    std::string default_value;
    int type = 0;
};

struct EffectDef {
    std::string name_ja;
    std::string name_en;
    std::string name_zh;
    std::vector<EffectParam> params;
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

extern ProjectState g_project_state;
extern SceneInfo g_scene_info;
extern EDIT_HANDLE* g_edit_handle;
extern LOG_HANDLE* g_logger;
extern HINSTANCE g_dll_hinst;

std::wstring utf8_to_wide(const std::string& utf8);
std::string wide_to_utf8(const std::wstring& wide);
std::string cp932_to_utf8(const std::string& cp932);
std::string maybe_cp932_to_utf8(const std::string& s);

bool parse_project_file(const std::wstring& file_path);
std::wstring get_project_summary();
void save_config_to_project_file(EDIT_HANDLE* edit, const OutputConfig& cfg);
void sync_scene_info();

void select_all_tracks();
void deselect_all_tracks();
void invert_track_selection();
