#include "effect_chain_editor.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/ui_config.h"
#include "plugin.h"
#include "exo/object_generator.h"
#include "effect/effect_dict.h"
#include <cstring>
#include <map>
#include <set>

bool g_show_effect_editor = false;
int g_current_template_idx = 0;
std::vector<ParsedEffect> g_template_effects;
std::vector<ParamBake> g_param_bakes;
std::vector<PresetEntry> g_presets;
std::string g_highlight_param_id;
int g_highlight_timer = 0;

std::vector<std::string> g_template_aliases;
bool g_template_effects_dirty = true;

// ---- 静态状态（持久到会话结束） ----
static std::vector<bool> s_header_states;

void refresh_template_effects() {
    g_template_effects_dirty = false;
    g_template_effects.clear();
    g_param_bakes.clear();
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

static int find_or_create_bake(int effect_index, const std::string& param_name, const std::string& default_val) {
    for (int i = 0; i < (int)g_param_bakes.size(); i++) {
        if (g_param_bakes[i].effect_index == effect_index && g_param_bakes[i].param_name == param_name) {
            return i;
        }
    }
    ParamBake pb;
    pb.effect_index = effect_index;
    pb.param_name = param_name;
    pb.param_value = default_val;
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

static void render_var_picker_button(const char* id) {
    if (ImGui::ArrowButton(id, ImGuiDir_Down)) {
        ImGui::OpenPopup(id);
    }
    if (ImGui::BeginPopup(id)) {
        ImGui::TextDisabled("TODO: 预设变量绑定");
        ImGui::EndPopup();
    }
}

static void render_bake_input(const std::string& label, std::string& value, const char* picker_id) {
    char buf[256];
    std::strncpy(buf, value.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::SetNextItemWidth(70);
    if (ImGui::InputText(label.c_str(), buf, sizeof(buf))) {
        value = buf;
    }
    ImGui::SameLine();
    render_var_picker_button(picker_id);
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
    if (g_template_aliases.size() > 1) {
        if (ImGui::BeginTabBar("Templates")) {
            for (int i = 0; i < (int)g_template_aliases.size(); i++) {
                std::string label = u8"模板" + std::to_string(i + 1);
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
        g_param_bakes.clear();
        s_header_states.clear();
        if (g_current_template_idx >= 0 && g_current_template_idx < (int)g_template_aliases.size()) {
            g_project_state.template_alias = g_template_aliases[g_current_template_idx];
        }
        refresh_template_effects();
    }

    // ---- 全部展开 / 全部收起 + 已勾选计数（同一行） ----
    static bool pending_expand_all = false;
    static bool pending_collapse_all = false;
    if (ImGui::SmallButton(u8"全部展开")) pending_expand_all = true;
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"全部收起")) pending_collapse_all = true;
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

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.45f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.50f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.20f, 0.40f, 1.0f));
            ImGui::Button("::", ImVec2(24, 0));
            ImGui::PopStyleColor(3);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                int payload = item.preset_index;
                ImGui::SetDragDropPayload("PRESET_REORDER", &payload, sizeof(int));
                ImGui::Text("%s", preset.display_name.c_str());
                ImGui::EndDragDropSource();
            }
            ImGui::SameLine();

            std::string label = preset.display_name + "##preset" + std::to_string(item.preset_index);
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.28f, 0.28f, 0.42f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.33f, 0.33f, 0.48f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.25f, 0.38f, 1.0f));
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
            if (s_header_states.size() <= (size_t)idx) s_header_states.resize(idx + 1, true);

            ImGui::PushID(("effect_hdr" + std::to_string(item.effect_index)).c_str());

            if (pending_expand_all) {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            } else if (pending_collapse_all || dragging) {
                ImGui::SetNextItemOpen(false, ImGuiCond_Always);
            } else {
                ImGui::SetNextItemOpen(s_header_states[idx], ImGuiCond_Always);
            }

            bool is_open = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            // 关键：只要不是拖拽中，就把 CollapsingHeader 的真实状态写回 s_header_states
            if (!dragging) {
                s_header_states[idx] = is_open;
            }

            if (is_open && !dragging) {
                std::set<std::string> processed;
                for (size_t pi = 0; pi < eff.params.size(); pi++) {
                    auto& param = eff.params[pi];
                    if (processed.count(param.first)) continue;

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
                    bool has_end = has_motion_pair(eff.params, param.first, &end_val);

                    ImGui::PushID((int)(item.effect_index * 1000 + pi));

                    int bake_start_idx = find_or_create_bake((int)item.effect_index, param.first, param.second);
                    bool active = g_param_bakes[bake_start_idx].active;

                    if (ImGui::Checkbox("##bake", &active)) {
                        g_param_bakes[bake_start_idx].active = active;
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted(param.first.c_str());
                    ImGui::SameLine();

                    if (active) {
                        render_bake_input("##val_start", g_param_bakes[bake_start_idx].param_value,
                                          "##picker_start");
                        if (has_end) {
                            ImGui::SameLine();
                            ImGui::TextUnformatted(u8"→");
                            ImGui::SameLine();

                            int bake_end_idx = find_or_create_bake((int)item.effect_index,
                                                                   param.first + ".1", end_val);
                            render_bake_input("##val_end", g_param_bakes[bake_end_idx].param_value,
                                              "##picker_end");
                        }
                    } else {
                        std::string display_value = param.second;
                        if (has_end) display_value += u8" → " + end_val;
                        ImGui::TextDisabled("= %s", display_value.c_str());
                    }

                    ImGui::PopID();
                    processed.insert(param.first);
                    if (has_end) processed.insert(param.first + ".1");
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
                IM_COL32(100, 200, 255, 255), 2.0f);
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
