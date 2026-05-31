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
    std::vector<double> midi_volume;     // CC7, 0-127, -1 = 未设置
    std::vector<double> midi_pan;        // CC10, 0-127 (64=center), -1 = 未设置
    std::vector<double> midi_pitch_bend; // -8192~8191, 0 = 中心
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

struct TemplateEntry {
    int object_id = 0;
    std::string alias;
    std::string display_name;
    int layer = 0;
    double sf = 0.0;
    std::string chain;
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

    // Phase 5: 物件与音符同步
    int sync_mode = 1;          // 0=与音符对齐, 1=拉伸到下一音符, 2=拉伸到固定值, 3=仅在间隙生成, 4=仅在间隙并拉伸固定值
    int fixed_duration_frames = 30;
    int layer_strategy = 0;     // 0=优化模式, 1=持续累加模式
    bool reverse_layer_order = false;
    int track_filter_mode = 0;  // 0=全部独立, 1=仅取第N轨, 2=仅取倒数第N轨
    int track_filter_n = 1;

    // Phase 4: 多源映射
    int mapping_strategy = 1;            // 1=顺序轮替, 2=随机抽选, 3=和弦映射
    int mapping_sequential_order = 0;    // 0=顺序, 1=倒序, 2=洗牌
    bool mapping_no_consecutive = false; // 随机抽选：禁止连续重复
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
    std::vector<std::wstring> file_history;
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
extern std::vector<TemplateEntry> g_template_pool;

std::wstring utf8_to_wide(const std::string& utf8);
std::string wide_to_utf8(const std::wstring& wide);
std::string cp932_to_utf8(const std::string& cp932);
std::string maybe_cp932_to_utf8(const std::string& s);

bool parse_project_file(const std::wstring& file_path);
std::wstring get_project_summary();
void save_config_to_project_file(EDIT_HANDLE* edit, const OutputConfig& cfg);
void load_project_state_from_project_file(EDIT_SECTION* edit);
void save_project_file_path_and_history(EDIT_SECTION* edit);
void flush_project_file_state(EDIT_SECTION* edit);
void add_file_to_history(const std::wstring& path);
void remove_file_from_history(int index);
void sync_scene_info();

std::string extract_template_chain(const std::string& alias);

void select_all_tracks();
void deselect_all_tracks();
void invert_track_selection();
