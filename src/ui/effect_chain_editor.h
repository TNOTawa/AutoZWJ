#pragma once
#include <vector>
#include <string>

// ---- 中间数据结构 ----
struct ParsedEffect {
    std::string effect_name;
    std::string effect_name_zh;
    int original_index = 0;
    std::vector<std::pair<std::string, std::string>> params;
};

struct ParamBake {
    int effect_index = -1;
    std::string param_name;
    std::string param_value;
    bool active = false;
};

struct PresetEntry {
    std::string display_name;
    std::string effect_block;
    int position = -1;
    bool active = false;
    // 点击预设后高亮参数面板中的对应控件（空字符串=无映射）
    std::string highlight_target;
};

// ---- 全局 UI 状态 ----
extern bool g_show_effect_editor;
extern int g_current_template_idx;
extern std::vector<ParsedEffect> g_template_effects;
extern std::vector<ParamBake> g_param_bakes;
extern std::vector<PresetEntry> g_presets;

// 参数面板高亮系统
extern std::string g_highlight_param_id;   // 当前需要高亮的参数面板控件 ID
extern int g_highlight_timer;                // 高亮倒计时（帧数）

// ---- 多模板数据 ----
extern std::vector<std::string> g_template_aliases;
extern bool g_template_effects_dirty;

// ---- 函数声明 ----
std::vector<ParsedEffect> parse_effect_chain(const std::string& alias_chain);
void refresh_template_effects();
void sync_presets_from_config();

// ---- 渲染 ----
void render_effect_chain_panel();
