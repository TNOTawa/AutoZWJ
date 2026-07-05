#include "effect_chain_editor.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/ui_config.h"
#include "plugin.h"
#include "chain/template_chain.h"
#include "exo/object_generator.h"
#include "effect/effect_dict.h"
#include <cstring>
#include <map>
#include <set>
#include <algorithm>
#include <unordered_map>

bool g_show_effect_editor = false;
int g_current_template_idx = 0;
std::vector<ParsedEffect> g_template_effects;
std::vector<ParamBake> g_param_bakes;
std::vector<PresetEntry> g_presets;
std::string g_highlight_param_id;
int g_highlight_timer = 0;

std::vector<std::string> g_template_aliases;
bool g_template_effects_dirty = true;

static bool GhostSmallButton(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::COL_BG_ELEVATED);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::COL_BG_SURFACE_ACTIVE);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    bool ret = ImGui::SmallButton(label);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return ret;
}

std::vector<std::vector<ParsedEffect>> g_template_effects_per_tpl;
std::vector<std::vector<ParamBake>> g_param_bakes_per_tpl;
std::vector<std::vector<PresetEntry>> g_template_presets_per_tpl;

static bool is_known_expr_function(const std::string& s) {
    size_t p = s.find('(');
    if (p == std::string::npos || p == 0) return false;
    std::string name = s.substr(0, p);
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
    static const char* known[] = {
        "abs","min","max","clamp","floor","ceil","round",
        "sin","cos","sqrt","pow","rand","rand_int","map_pitch"
    };
    for (const char* k : known) {
        if (name == k) return true;
    }
    return false;
}

static int detect_mode_from_text(const std::string& s) {
    size_t c = std::count(s.begin(), s.end(), '$');
    if (c == 0) {
        // 无 $ 但看起来像函数调用 → 表达式
        if (is_known_expr_function(s)) return 2;
        return 0;
    }
    if (c == 2 && s.size() >= 2 && s.front() == '$' && s.back() == '$') return 1;
    return 2;
}

static const char* get_mode_label(int mode) {
    switch (mode) {
        case 0: return u8"[固]";
        case 1: return u8"[变]";
        case 2: return u8"[表]";
        default: return "";
    }
}

// ---- 静态状态（持久到会话结束） ----
static std::vector<bool> s_header_states;

// 输入框动画状态（全局，供聚焦检测使用）
static std::unordered_map<std::string, float> s_input_anim_widths;
static std::unordered_map<std::string, float> s_input_anim_heights;

void save_current_template_data() {
    if (g_current_template_idx < 0 || g_current_template_idx >= (int)g_template_pool.size()) return;
    if ((int)g_template_effects_per_tpl.size() < (int)g_template_pool.size()) {
        g_template_effects_per_tpl.resize(g_template_pool.size());
        g_param_bakes_per_tpl.resize(g_template_pool.size());
        g_template_presets_per_tpl.resize(g_template_pool.size());
    }
    for (auto& pb : g_param_bakes) {
        pb.value_mode = detect_mode_from_text(pb.param_value);
    }
    g_template_effects_per_tpl[g_current_template_idx] = g_template_effects;
    g_param_bakes_per_tpl[g_current_template_idx] = g_param_bakes;
    g_template_presets_per_tpl[g_current_template_idx] = g_presets;
}

void load_template_data(int idx) {
    if (idx < 0 || idx >= (int)g_template_pool.size()) return;
    if ((int)g_template_effects_per_tpl.size() < (int)g_template_pool.size()) {
        g_template_effects_per_tpl.resize(g_template_pool.size());
        g_param_bakes_per_tpl.resize(g_template_pool.size());
        g_template_presets_per_tpl.resize(g_template_pool.size());
    }
    g_template_effects = g_template_effects_per_tpl[idx];
    g_param_bakes = g_param_bakes_per_tpl[idx];
    g_presets = g_template_presets_per_tpl[idx];
}

void refresh_template_effects() {
    g_template_effects_dirty = false;
    g_template_effects.clear();
    s_header_states.clear();

    if (g_current_template_idx < 0 || g_current_template_idx >= (int)g_template_aliases.size())
        return;

    std::string chain = extract_template_chain(g_template_aliases[g_current_template_idx]);
    if (chain.empty()) return;

    g_template_effects = parse_effect_chain(chain);
}

void sync_presets_from_config() {
    OutputConfig& cfg = g_project_state.config;

    std::map<std::string, int> saved_positions;
    for (const auto& p : g_presets) {
        if (p.active) {
            saved_positions[p.display_name] = p.position;
        }
    }

    g_presets.clear();

    if (cfg.alt_flip && cfg.flip_type != FLIP_NONE) {
        PresetEntry preset;
        switch (cfg.flip_type) {
            case FLIP_HORIZONTAL: preset.display_name = u8"左右翻转"; break;
            case FLIP_VERTICAL:   preset.display_name = u8"上下翻转"; break;
            case FLIP_CW:         preset.display_name = u8"顺时针旋转"; break;
            case FLIP_CCW:        preset.display_name = u8"逆时针旋转"; break;
            default:              preset.display_name = u8"翻转"; break;
        }
        preset.effect_block = "";
        preset.highlight_target = "flip_config";

        auto it = saved_positions.find(preset.display_name);
        preset.position = (it != saved_positions.end()) ? it->second : -1;
        preset.active = true;
        g_presets.push_back(preset);
    }
}

static bool is_hidden_param(const std::string& name) {
    if (name == "Group" || name == "Group2") return true;
    if (name.find(u8"のプリセット") != std::string::npos) return true;
    if (name.find("Preset") != std::string::npos) return true;
    return false;
}

static bool parse_motion_value(const std::string& val,
                                std::string* out_start,
                                std::string* out_end,
                                std::string* out_rest)
{
    size_t c1 = val.find(',');
    if (c1 == std::string::npos) return false;
    size_t c2 = val.find(',', c1 + 1);
    if (c2 == std::string::npos) return false;

    std::string start = val.substr(0, c1);
    std::string end = val.substr(c1 + 1, c2 - c1 - 1);

    auto trim = [](std::string& s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    };
    trim(start);
    trim(end);

    try {
        std::stod(start);
        std::stod(end);
    } catch (...) {
        return false;
    }

    if (out_start) *out_start = start;
    if (out_end) *out_end = end;
    if (out_rest) *out_rest = val.substr(c2);
    return true;
}

static int find_or_create_bake(int effect_index, const std::string& param_name,
                                const std::string& default_val,
                                bool is_motion = false,
                                const std::string& motion_end = "")
{
    for (int i = 0; i < (int)g_param_bakes.size(); i++) {
        if (g_param_bakes[i].effect_index == effect_index && g_param_bakes[i].param_name == param_name) {
            return i;
        }
    }
    ParamBake pb;
    pb.effect_index = effect_index;
    pb.param_name = param_name;
    pb.param_value = default_val;
    pb.is_motion = is_motion;
    pb.motion_end_value = motion_end;
    pb.active = false;
    g_param_bakes.push_back(pb);
    return (int)g_param_bakes.size() - 1;
}

static bool has_motion_pair(const std::vector<std::pair<std::string, std::string>>& params, const std::string& name, std::string* out_end_val) {
    std::string end_name = name + ".1";
    for (const auto& p : params) {
        if (p.first == end_name) {
            if (out_end_val) *out_end_val = p.second;
            return true;
        }
    }
    return false;
}

static int count_active_bakes_for_effect(int effect_index) {
    int count = 0;
    for (const auto& pb : g_param_bakes) {
        if (pb.effect_index == effect_index && pb.active) count++;
    }
    return count;
}

static int count_total_active_bakes() {
    int count = 0;
    for (const auto& pb : g_param_bakes) {
        if (pb.active) count++;
    }
    return count;
}

static void render_var_picker_button(const char* id, std::string& target_value) {
    if (ImGui::ArrowButton(id, ImGuiDir_Down)) {
        ImGui::OpenPopup(id);
    }
    if (ImGui::BeginPopup(id)) {
        auto var_item = [&](const char* label, const char* insert_text) {
            ImGui::PushStyleColor(ImGuiCol_Text, UI::COL_TEXT_SECONDARY);
            if (ImGui::Selectable(label)) {
                target_value = insert_text;
            }
            ImGui::PopStyleColor();
        };
        ImGui::TextColored(UI::COL_TEXT_PRIMARY, "note.*");
        var_item("note.pitch",        "$note.pitch$");
        var_item("note.velocity",     "$note.velocity$");
        var_item("note.index",        "$note.index$");
        var_item("note.index0",       "$note.index0$");
        var_item("note.duration",     "$note.duration$");
        var_item("note.duration_sec", "$note.duration_sec$");
        var_item("note.start_frame",  "$note.start_frame$");
        var_item("note.end_frame",    "$note.end_frame$");
        var_item("note.pitch_ratio",  "$note.pitch_ratio$");
        var_item("note.pitch_min",    "$note.pitch_min$");
        var_item("note.pitch_max",    "$note.pitch_max$");
        var_item("note.bpm",          "$note.bpm$");
        ImGui::Separator();
        ImGui::TextColored(UI::COL_TEXT_PRIMARY, "item.*");
        var_item("item.position", "$item.position$");
        var_item("item.length",   "$item.length$");
        var_item("item.playrate", "$item.playrate$");
        var_item("item.loop",     "$item.loop$");
        var_item("item.soffs",    "$item.soffs$");
        ImGui::Separator();
        ImGui::TextColored(UI::COL_TEXT_PRIMARY, "midi.*");
        var_item("midi.volume",     "$midi.volume$");
        var_item("midi.pan",        "$midi.pan$");
        var_item("midi.pitch_bend", "$midi.pitch_bend$");
        ImGui::Separator();
        ImGui::TextColored(UI::COL_TEXT_PRIMARY, "track.*");
        var_item("track.number", "$track.number$");
        ImGui::Separator();
        ImGui::TextColored(UI::COL_TEXT_PRIMARY, "global.*");
        var_item("global.fps",        "$global.fps$");
        var_item("global.width",      "$global.width$");
        var_item("global.height",     "$global.height$");
        var_item("global.item_count", "$global.item_count$");
        ImGui::Separator();
        ImGui::TextColored(UI::COL_TEXT_PRIMARY, "chord.*");
        var_item("chord.index", "$chord.index$");
        var_item("chord.count", "$chord.count$");
        ImGui::Separator();
        ImGui::TextColored(UI::COL_TEXT_PRIMARY, u8"文本");
        var_item("note.lyric", "$note.lyric$");
        ImGui::Separator();
        ImGui::TextColored(UI::COL_TEXT_PRIMARY, u8"函数");
        var_item("rand(0, 127)",       "rand(0, 127)");
        var_item("rand_int(0, 4)",     "rand_int(0, 4)");
        var_item("map_pitch(0, 100)",  "map_pitch(0, 100)");
        var_item("abs(x)",             "abs(x)");
        var_item("clamp(x, lo, hi)",   "clamp(x, lo, hi)");
        var_item("round(x)",           "round(x)");
        ImGui::EndPopup();
    }
}

static bool is_input_focused(const std::string& anim_key, float normal_w) {
    auto it = s_input_anim_widths.find(anim_key);
    if (it == s_input_anim_widths.end()) return false;
    return it->second > normal_w + 8.0f;
}

static void render_bake_input(const std::string& label, std::string& value,
                               const char* picker_id, const std::string& anim_key,
                               float normal_w, float max_w)
{
    float& cur_w = s_input_anim_widths[anim_key];
    float& cur_h = s_input_anim_heights[anim_key];

    float frame_h = ImGui::GetFrameHeight();

    float render_w = cur_w > 10.0f ? cur_w : normal_w;
    float render_h = cur_h > 5.0f ? cur_h : frame_h;

    if (render_w > max_w) render_w = max_w;

    ImGui::SetNextItemWidth(render_w);

    char buf[512];
    std::strncpy(buf, value.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    ImVec2 size(render_w, render_h);
    bool changed = ImGui::InputTextMultiline(label.c_str(), buf, sizeof(buf), size);
    if (changed) value = buf;

    bool is_active = ImGui::IsItemActive();

    float target_w = is_active ? max_w : normal_w;
    float target_h = is_active ? frame_h * 2.4f : frame_h;

    float dt = ImGui::GetIO().DeltaTime;
    float speed = 1.0f - std::pow(0.001f, dt * 14.0f);
    cur_w += (target_w - cur_w) * speed;
    cur_h += (target_h - cur_h) * speed;

    if (std::abs(cur_w - target_w) < 0.5f) cur_w = target_w;
    if (std::abs(cur_h - target_h) < 0.5f) cur_h = target_h;

    ImGui::SameLine();
    render_var_picker_button(picker_id, value);
}

struct DisplayItem {
    bool is_preset;
    int effect_index;
    int preset_index;
};

struct ItemRect {
    float min_y;
    float max_y;
};

static int get_effect_position_before(size_t idx, const std::vector<DisplayItem>& items) {
    int count = 0;
    for (size_t i = 0; i < idx && i < items.size(); i++) {
        if (!items[i].is_preset) count++;
    }
    return count;
}

static int get_effect_position_after(size_t idx, const std::vector<DisplayItem>& items) {
    int count = 0;
    for (size_t i = 0; i <= idx && i < items.size(); i++) {
        if (!items[i].is_preset) count++;
    }
    return count;
}

static int calculate_insert_position(float mouse_y, const std::vector<ItemRect>& rects,
                                      const std::vector<DisplayItem>& items) {
    if (rects.empty()) return -1;
    if (mouse_y <= rects[0].min_y) return 0;
    if (mouse_y >= rects.back().max_y) return -1;

    for (size_t i = 0; i < rects.size(); i++) {
        float center = (rects[i].min_y + rects[i].max_y) * 0.5f;
        if (mouse_y >= rects[i].min_y && mouse_y < rects[i].max_y) {
            if (mouse_y < center) {
                return get_effect_position_before(i, items);
            } else {
                return get_effect_position_after(i, items);
            }
        }
        if (i + 1 < rects.size() && mouse_y >= rects[i].max_y && mouse_y < rects[i + 1].min_y) {
            return get_effect_position_before(i + 1, items);
        }
    }
    return -1;
}

static float calculate_line_y(float mouse_y, const std::vector<ItemRect>& rects) {
    if (rects.empty()) return 0.0f;
    if (mouse_y <= rects[0].min_y) return rects[0].min_y;
    if (mouse_y >= rects.back().max_y) return rects.back().max_y;

    for (size_t i = 0; i < rects.size(); i++) {
        float center = (rects[i].min_y + rects[i].max_y) * 0.5f;
        if (mouse_y >= rects[i].min_y && mouse_y < rects[i].max_y) {
            if (mouse_y < center) return rects[i].min_y;
            else return rects[i].max_y;
        }
        if (i + 1 < rects.size() && mouse_y >= rects[i].max_y && mouse_y < rects[i + 1].min_y) {
            return (rects[i].max_y + rects[i + 1].min_y) * 0.5f;
        }
    }
    return rects.back().max_y;
}

void render_effect_chain_panel() {
    if (g_highlight_timer > 0) g_highlight_timer--;

    if (g_template_aliases.empty()) {
        ImGui::TextDisabled(u8"未选择模板物件");
        return;
    }

    // 懒加载：需要刷新时解析模板
    if (g_template_effects_dirty && !g_template_aliases.empty()) {
        refresh_template_effects();
    }

    // ---- 同步预设（拖拽过程中跳过以避免干扰） ----
    if (ImGui::GetDragDropPayload() == nullptr) {
        sync_presets_from_config();
    }

    // ---- TabBar（多模板时显示） ----
    bool template_changed = false;
    int prev_template_idx = g_current_template_idx;
    if (g_template_aliases.size() > 1) {
        if (ImGui::BeginTabBar("Templates")) {
            for (int i = 0; i < (int)g_template_aliases.size(); i++) {
                std::string label = u8"模板" + std::to_string(i + 1);
                if (i < (int)g_template_pool.size() && !g_template_pool[i].display_name.empty()) {
                    label = g_template_pool[i].display_name;
                    if (label.size() > 16) label = label.substr(0, 13) + "...";
                }
                if (ImGui::BeginTabItem(label.c_str())) {
                    if (g_current_template_idx != i) {
                        g_current_template_idx = i;
                        template_changed = true;
                    }
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    if (template_changed) {
        int new_idx = g_current_template_idx;
        g_current_template_idx = prev_template_idx;
        save_current_template_data();
        g_current_template_idx = new_idx;
        s_header_states.clear();
        if (g_current_template_idx >= 0 && g_current_template_idx < (int)g_template_aliases.size()) {
            g_project_state.template_alias = g_template_aliases[g_current_template_idx];
        }
        load_template_data(g_current_template_idx);
        refresh_template_effects();
    }

    // ---- 全部展开 / 全部收起 + 已勾选计数（同一行） ----
    static bool pending_expand_all = false;
    static bool pending_collapse_all = false;
    if (GhostSmallButton(u8"全部展开")) pending_expand_all = true;
    ImGui::SameLine();
    if (GhostSmallButton(u8"全部收起")) pending_collapse_all = true;
    int total = count_total_active_bakes();
    if (total > 0) {
        ImGui::SameLine();
        ImGui::TextDisabled(u8"已勾选 %d 个参数", total);
    }
    ImGui::Spacing();

    // ---- 拖拽状态 ----
    bool dragging = (ImGui::GetDragDropPayload() != nullptr);

    // ---- 构建混合显示列表 ----
    std::vector<DisplayItem> items;
    std::map<int, std::vector<int>> preset_groups;
    for (int i = 0; i < (int)g_presets.size(); i++) {
        if (g_presets[i].active) {
            preset_groups[g_presets[i].position].push_back(i);
        }
    }
    for (int pi : preset_groups[0]) {
        items.push_back({true, -1, pi});
    }
    for (size_t ei = 0; ei < g_template_effects.size(); ei++) {
        items.push_back({false, (int)ei, -1});
        int next_pos = (int)ei + 1;
        auto it = preset_groups.find(next_pos);
        if (it != preset_groups.end()) {
            for (int pi : it->second) {
                items.push_back({true, -1, pi});
            }
        }
    }
    for (int pi : preset_groups[-1]) {
        items.push_back({true, -1, pi});
    }

    // ---- 渲染统一列表，同时记录 Y 范围 ----
    std::vector<ItemRect> item_rects;
    item_rects.reserve(items.size());

    for (size_t i = 0; i < items.size(); i++) {
        auto& item = items[i];
        if (item.is_preset) {
            auto& preset = g_presets[item.preset_index];
            ImGui::PushID(("preset_row" + std::to_string(item.preset_index)).c_str());

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::COL_BG_ELEVATED);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::COL_BG_SURFACE_ACTIVE);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::Button(("##grip" + std::to_string(item.preset_index)).c_str(), ImVec2(28, 0));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImVec2 rmin = ImGui::GetItemRectMin();
            ImVec2 rmax = ImGui::GetItemRectMax();
            ImVec2 c((rmin.x + rmax.x) * 0.5f, (rmin.y + rmax.y) * 0.5f);
            ImU32 dcol = ImGui::GetColorU32(ImGui::IsItemHovered() ? UI::COL_TEXT_PRIMARY : UI::COL_TEXT_SECONDARY);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float aw = 4.0f, ah = 3.0f;
            // Up arrow
            dl->AddTriangleFilled(
                ImVec2(c.x, c.y - ah - 1),
                ImVec2(c.x - aw, c.y - 1),
                ImVec2(c.x + aw, c.y - 1),
                dcol);
            // Down arrow
            dl->AddTriangleFilled(
                ImVec2(c.x, c.y + ah + 1),
                ImVec2(c.x - aw, c.y + 1),
                ImVec2(c.x + aw, c.y + 1),
                dcol);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                int payload = item.preset_index;
                ImGui::SetDragDropPayload("PRESET_REORDER", &payload, sizeof(int));
                ImGui::Text("%s", preset.display_name.c_str());
                ImGui::EndDragDropSource();
            }
            ImGui::SameLine();

            std::string label = preset.display_name + "##preset" + std::to_string(item.preset_index);
            ImGui::PushStyleColor(ImGuiCol_Header, UI::COL_BG_ELEVATED);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, UI::COL_BG_SURFACE_HOVER);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, UI::COL_BG_SURFACE_ACTIVE);
            if (ImGui::Selectable(label.c_str(), false,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                if (!preset.highlight_target.empty()) {
                    g_highlight_param_id = preset.highlight_target;
                    g_highlight_timer = 30;
                }
            }
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        } else {
            auto& eff = g_template_effects[item.effect_index];
            int active_count = count_active_bakes_for_effect((int)item.effect_index);
            std::string label = (eff.effect_name_zh.empty() ? eff.effect_name : eff.effect_name_zh);
            if (active_count > 0) label += " (" + std::to_string(active_count) + ")";
            label += "##effect" + std::to_string(item.effect_index);

            int idx = item.effect_index;
            if (s_header_states.size() <= (size_t)idx) s_header_states.resize(idx + 1, false);

            ImGui::PushID(("effect_hdr" + std::to_string(item.effect_index)).c_str());

            if (pending_expand_all) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            } else if (pending_collapse_all || dragging) {
                ImGui::SetNextItemOpen(false, ImGuiCond_Always);
            } else {
                ImGui::SetNextItemOpen(s_header_states[idx], ImGuiCond_Always);
            }

            bool is_open = ImGui::CollapsingHeader(label.c_str());

            // 关键：只要不是拖拽中，就把 CollapsingHeader 的真实状态写回 s_header_states
            if (!dragging) {
                s_header_states[idx] = is_open;
            }

            if (is_open && !dragging) {
                std::set<std::string> processed;
                for (size_t pi = 0; pi < eff.params.size(); pi++) {
                    auto& param = eff.params[pi];
                    if (processed.count(param.first)) continue;

                    // 隐藏参数过滤
                    if (is_hidden_param(param.first)) {
                        processed.insert(param.first);
                        continue;
                    }

                    if (param.first.size() > 2 &&
                        param.first.substr(param.first.size() - 2) == ".1") {
                        std::string base = param.first.substr(0, param.first.size() - 2);
                        bool base_exists = false;
                        for (const auto& pp : eff.params) {
                            if (pp.first == base) { base_exists = true; break; }
                        }
                        if (base_exists) {
                            processed.insert(param.first);
                            continue;
                        }
                    }

                    std::string end_val;
                    bool has_dot1_end = has_motion_pair(eff.params, param.first, &end_val);

                    std::string motion_start, motion_end, motion_rest;
                    bool is_comma_motion = false;
                    if (!has_dot1_end) {
                        is_comma_motion = parse_motion_value(param.second, &motion_start, &motion_end, &motion_rest);
                    }

                    ImGui::PushID((int)(item.effect_index * 1000 + pi));

                    int bake_idx = find_or_create_bake((int)item.effect_index, param.first,
                                                        is_comma_motion ? motion_start : param.second,
                                                        is_comma_motion, is_comma_motion ? motion_end : "");
                    bool active = g_param_bakes[bake_idx].active;

                    if (ImGui::Checkbox("##bake", &active)) {
                        g_param_bakes[bake_idx].active = active;
                    }

                    // 参数名截断，防止超长导致横向溢出
                    auto truncate_name = [](const std::string& s, float max_w) -> std::string {
                        float w = ImGui::CalcTextSize(s.c_str()).x;
                        if (w <= max_w) return s;
                        std::string r = s;
                        while (!r.empty()) {
                            unsigned char c = r.back();
                            r.pop_back();
                            while (!r.empty() && (static_cast<unsigned char>(r.back()) & 0x80) && !(static_cast<unsigned char>(r.back()) & 0x40)) {
                                r.pop_back();
                            }
                            if (ImGui::CalcTextSize((r + "...").c_str()).x <= max_w) {
                                return r + "...";
                            }
                        }
                        return "...";
                    };
                    std::string display_name = truncate_name(param.first, 70.0f);

                    ImGui::SameLine();
                    ImGui::TextUnformatted(display_name.c_str());

                    if (!active) {
                        ImGui::SameLine();
                        std::string display_value = param.second;
                        if (has_dot1_end) display_value += u8" → " + end_val;
                        ImGui::TextDisabled("= %s", display_value.c_str());
                    } else {
                        ImGui::SameLine();
                        float input_start_x = ImGui::GetCursorPosX();

                        float avail = ImGui::GetContentRegionAvail().x;
                        float spacing = ImGui::GetStyle().ItemSpacing.x;
                        float picker_w = 24.0f + spacing;
                        float max_w = avail - picker_w - 6.0f;
                        if (max_w < 40.0f) max_w = 40.0f;
                        float normal_w = max_w * 0.5f;

                        std::string start_key = std::to_string(item.effect_index) + ":" + param.first + ":start";
                        std::string end_key;
                        if (is_comma_motion) {
                            end_key = std::to_string(item.effect_index) + ":" + param.first + ":end";
                        } else if (has_dot1_end) {
                            end_key = std::to_string(item.effect_index) + ":" + param.first + ".1";
                        }

                        bool has_double = is_comma_motion || has_dot1_end;

                        if (has_double) {
                            // 第一行：起点
                            render_bake_input("##val_start", g_param_bakes[bake_idx].param_value,
                                              "##picker_start", start_key, normal_w, max_w);

                            // 第二行：终点，对齐到第一行输入框
                            ImGui::SetCursorPosX(input_start_x);
                            float avail2 = ImGui::GetContentRegionAvail().x;
                            float max_w2 = avail2 - picker_w - 6.0f;
                            if (max_w2 < 40.0f) max_w2 = 40.0f;
                            float normal_w2 = max_w2 * 0.5f;

                            if (has_dot1_end) {
                                int bake_end_idx = find_or_create_bake((int)item.effect_index,
                                                                       param.first + ".1", end_val);
                                render_bake_input("##val_end", g_param_bakes[bake_end_idx].param_value,
                                                  "##picker_end", end_key, normal_w2, max_w2);
                            } else {
                                render_bake_input("##val_end", g_param_bakes[bake_idx].motion_end_value,
                                                  "##picker_end", end_key, normal_w2, max_w2);
                            }
                            if (!motion_rest.empty()) {
                                ImGui::SameLine();
                                ImGui::TextDisabled("%s", motion_rest.c_str());
                            }
                        } else {
                            render_bake_input("##val", g_param_bakes[bake_idx].param_value,
                                              "##picker", start_key, normal_w, max_w);
                        }
                    }

                    ImGui::PopID();
                    processed.insert(param.first);
                    if (has_dot1_end) processed.insert(param.first + ".1");
                }
            }
            ImGui::PopID();
        }

        ItemRect r;
        r.min_y = ImGui::GetItemRectMin().y;
        r.max_y = ImGui::GetItemRectMax().y;
        item_rects.push_back(r);
    }

    pending_expand_all = false;
    pending_collapse_all = false;

    // ---- 全窗口范围拖拽接收（BeginDragDropTargetCustom） ----
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();
    ImRect drop_rect(win_pos, ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y));

    if (ImGui::BeginDragDropTargetCustom(drop_rect, ImGui::GetID("##effect_chain_panel"))) {
        float mouse_y = ImGui::GetMousePos().y;
        float line_y = calculate_line_y(mouse_y, item_rects);

        if (!item_rects.empty()) {
            float x1 = win_pos.x + ImGui::GetWindowContentRegionMin().x;
            float x2 = win_pos.x + ImGui::GetWindowContentRegionMax().x;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(x1, line_y), ImVec2(x2, line_y),
                UI::COL_DRAG_INSERT, 2.0f);
        }

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PRESET_REORDER")) {
            int src_idx = *(const int*)payload->Data;
            if (src_idx >= 0 && src_idx < (int)g_presets.size()) {
                g_presets[src_idx].position = calculate_insert_position(mouse_y, item_rects, items);
            }
        }
        ImGui::EndDragDropTarget();
    }
}
