#pragma once
#include <vector>
#include <string>
#include "core/effect_model.h"

// ---- 全局 UI 状态 ----
extern bool g_show_effect_editor;
extern std::vector<ParsedEffect> g_template_effects;
extern std::vector<ParamBake> g_param_bakes;
extern std::vector<PresetEntry> g_presets;

// 参数面板高亮系统
extern std::string g_highlight_param_id;   // 当前需要高亮的参数面板控件 ID
extern int g_highlight_timer;                // 高亮倒计时（帧数）

// ---- 多模板数据 ----
extern bool g_template_effects_dirty;

// ---- 函数声明 ----
void refresh_template_effects();
void sync_presets_from_config();
void save_current_template_data();
void load_template_data(int idx);

// ---- 渲染 ----
void render_effect_chain_panel();