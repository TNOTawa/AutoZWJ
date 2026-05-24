#include "ui_components.h"
#include "effect/effect_dict.h"
#include "ui/file_picker.h"
#include "ui/imgui_window.h"
#include "exo/object_generator.h"
#include <algorithm>

void render_header_panel() {
    if (!g_project_state.has_data) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
            u8"未加载音频工程");
        return;
    }
    std::wstring summary = get_project_summary();
    std::string summary_utf8 = wide_to_utf8(summary);
    ImGui::Text("%s", summary_utf8.c_str());
}

static void render_track_node(TrackNode& node, int depth) {
    ImGui::PushID(node.number);

    std::string label = std::to_string(node.number) + ": " + wide_to_utf8(node.name);
    if (label.empty()) label = "Track " + std::to_string(node.number);

    if (node.is_bus) {
        label = u8"📁 " + label;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        ImGui::Checkbox("##sel", &node.selected);
        ImGui::SameLine();
        bool open = ImGui::TreeNodeEx(label.c_str(), flags);
        if (open) {
            for (auto& child : node.children) {
                render_track_node(child, depth + 1);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        return;
    }

    if (depth > 0) {
        std::string prefix(depth * 2, ' ');
        label = prefix + label;
    }

    ImGui::Checkbox("##sel", &node.selected);
    ImGui::SameLine();
    ImGui::TextUnformatted(label.c_str());

    ImGui::PopID();
}

void render_track_tree() {
    if (!g_project_state.has_data) return;
    ImGui::Text(u8"轨道选择");
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"全选")) select_all_tracks();
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"取消")) deselect_all_tracks();
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"反选")) invert_track_selection();
    ImGui::Separator();
    for (auto& track : g_project_state.tracks) {
        render_track_node(track, 0);
    }
}

void render_config_panel() {
    OutputConfig& cfg = g_project_state.config;
    SceneInfo& info = g_scene_info;

    ImGui::Text(u8"场景信息");
    ImGui::Separator();
    ImGui::Text("%d x %d @ %d/%d fps  |  %d Hz",
        info.width, info.height, info.rate, info.scale, info.sample_rate);

    if (g_scene_info.valid) {
        cfg.fps_num = info.rate;
        cfg.fps_den = info.scale;
    }

    ImGui::Spacing();
    ImGui::Text(u8"参数设置");
    ImGui::Separator();

    ImGui::Checkbox(u8"交替翻转", &cfg.alt_flip);
    if (cfg.alt_flip) {
        ImGui::Indent(16);
        static const char* flip_types[] = {
            u8"左右翻转",
            u8"上下翻转",
            u8"顺时针旋转",
            u8"逆时针旋转"
        };
        int flip_idx = 0;
        if (cfg.flip_type == FLIP_HORIZONTAL) flip_idx = 0;
        else if (cfg.flip_type == FLIP_VERTICAL) flip_idx = 1;
        else if (cfg.flip_type == FLIP_CW) flip_idx = 2;
        else if (cfg.flip_type == FLIP_CCW) flip_idx = 3;
        ImGui::SetNextItemWidth(120);
        ImGui::Combo(u8"翻转方向", &flip_idx, flip_types, 4);
        if (flip_idx == 0) cfg.flip_type = FLIP_HORIZONTAL;
        else if (flip_idx == 1) cfg.flip_type = FLIP_VERTICAL;
        else if (flip_idx == 2) cfg.flip_type = FLIP_CW;
        else cfg.flip_type = FLIP_CCW;
        ImGui::Unindent(16);
    }

    ImGui::Checkbox(u8"偶数项换行", &cfg.even_only);
    ImGui::Checkbox(u8"无节拍同步", &cfg.beatless_sync);
    ImGui::Checkbox(u8"填充间隙", &cfg.no_gap);
    ImGui::Checkbox(u8"向上取整帧", &cfg.use_round_up);
}

void render_effect_picker() {
    ImGui::Text(u8"效果选择");

    auto& reg = get_effect_registry();
    float window_width = ImGui::GetContentRegionAvail().x;
    int columns = std::max(2, (int)(window_width / 150.0f));

    int count = 0;
    for (auto& eff : reg) {
        ImGui::PushID(eff.name_en.c_str());
        bool sel = g_project_state.config.selected_effects.end() !=
            std::find(g_project_state.config.selected_effects.begin(),
                g_project_state.config.selected_effects.end(), eff.name_en);
        if (ImGui::Checkbox(eff.name_ja.c_str(), &sel)) {
            if (sel) g_project_state.config.selected_effects.push_back(eff.name_en);
            else {
                auto it = std::find(g_project_state.config.selected_effects.begin(),
                    g_project_state.config.selected_effects.end(), eff.name_en);
                if (it != g_project_state.config.selected_effects.end())
                    g_project_state.config.selected_effects.erase(it);
            }
        }
        ImGui::PopID();
        count++;
        if (count % columns != 0) ImGui::SameLine();
    }
}

void render_action_bar() {
    ImGui::Separator();
    if (ImGui::Button(u8"重新选择文件...", ImVec2(140, 0))) {
        imgui_window_hide();
        show_file_picker(nullptr, [](const std::wstring& path) {
            if (parse_project_file(path)) {
                imgui_window_show();
            }
        });
    }
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
    if (ImGui::Button(u8"执行导入生成", ImVec2(110, 30))) {
        if (g_project_state.has_data)
            imgui_window_trigger_generate();
    }
}
