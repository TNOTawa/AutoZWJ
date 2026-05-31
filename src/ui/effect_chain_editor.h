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
    std::string param_value;   // 用户输入的原始文本（固定值/变量/表达式）
    int value_mode = 0;        // 0=固定值, 1=变量映射, 2=表达式
    bool active = false;
    bool is_motion = false;         // 是否为逗号分隔的运动参数
    std::string motion_end_value;   // 运动参数的终点值（第二个字段）
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

// 每模板独立的数据（二维数组，索引与 g_template_pool / g_template_aliases 对齐）
extern std::vector<std::vector<ParsedEffect>> g_template_effects_per_tpl;
extern std::vector<std::vector<ParamBake>> g_param_bakes_per_tpl;
extern std::vector<std::vector<PresetEntry>> g_template_presets_per_tpl;

// ---- 函数声明 ----
std::vector<ParsedEffect> parse_effect_chain(const std::string& alias_chain);
void refresh_template_effects();
void sync_presets_from_config();
void save_current_template_data();
void load_template_data(int idx);

// ---- 渲染 ----
void render_effect_chain_panel();
