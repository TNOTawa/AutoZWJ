#include "ui_components.h"
#include "ui/ui_config.h"
#include "effect/effect_dict.h"
#include "ui/file_picker.h"
#include "ui/imgui_window.h"
#include "ui/effect_chain_editor.h"
#include "exo/object_generator.h"
#include <algorithm>
#include <sstream>

AppPage g_current_page = AppPage::Config;

// ---------------------------------------------------------------------------
// 目录持久化
// ---------------------------------------------------------------------------
std::wstring load_last_directory() {
    return g_project_state.last_directory;
}

void save_last_directory(const std::wstring& dir) {
    g_project_state.last_directory = dir;
}

// ---------------------------------------------------------------------------
// REAPER.ini 最近文件（直接读取原始字节，自适应编码）
// ---------------------------------------------------------------------------
static std::vector<std::wstring> get_reaper_recent_files_internal() {
    std::vector<std::wstring> result;

    wchar_t ini_path_w[MAX_PATH] = {};
    ExpandEnvironmentStringsW(L"%APPDATA%\\REAPER\\REAPER.ini", ini_path_w, MAX_PATH);

    HANDLE hFile = CreateFileW(ini_path_w, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return result;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0) { CloseHandle(hFile); return result; }
    std::string raw((size_t)size.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, &raw[0], (DWORD)size.QuadPart, &read, nullptr);
    CloseHandle(hFile);
    if (!ok || read != (DWORD)size.QuadPart) return result;
    raw.resize(read);

    std::string content;
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        content = raw.substr(3);
    } else if (raw.size() >= 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE) {
        int len = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)(raw.c_str() + 2), -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            content.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)(raw.c_str() + 2), -1, &content[0], len, nullptr, nullptr);
        }
    } else {
        content = raw;
        if (!is_valid_utf8(content)) {
            content = maybe_cp932_to_utf8(content);
        }
    }

    struct RecentEntry {
        int num;
        std::wstring path;
    };
    std::vector<RecentEntry> entries;

    std::istringstream iss(content);
    std::string line;
    bool in_recent = false;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "[Recent]") {
            in_recent = true;
            continue;
        }
        if (in_recent) {
            if (!line.empty() && line[0] == '[') {
                in_recent = false;
                continue;
            }
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                if (key.find("recent") == 0) {
                    int num = std::atoi(key.c_str() + 6);
                    std::string val = line.substr(eq + 1);
                    entries.push_back({num, utf8_to_wide(val)});
                }
            }
        }
    }

    std::sort(entries.begin(), entries.end(), [](const RecentEntry& a, const RecentEntry& b) {
        return a.num > b.num;
    });
    for (auto& e : entries) result.push_back(e.path);
    return result;
}

// ---------------------------------------------------------------------------
// 幽灵按钮（暗底 + 白字 + 白边框）
// ---------------------------------------------------------------------------
static bool GhostButton(const char* label, const ImVec2& size = ImVec2(0,0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::COL_BG_ELEVATED);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::COL_BG_SURFACE_ACTIVE);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    bool ret = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return ret;
}

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

// ---------------------------------------------------------------------------
// 轨道树（复用组件，制表符直角分支可视化）
// ---------------------------------------------------------------------------
static void render_flat_track_node(size_t idx, bool read_only) {
    auto& tracks = g_project_state.tracks;
    TrackNode& node = tracks[idx];
    int d = node.depth;
    bool is_empty = (node.count <= 0);

    ImGui::PushID(node.number);

    // 构建制表符前缀
    std::string prefix;
    for (int level = 0; level < d; level++) {
        bool ancestor_has_next = false;
        int ancestor_idx = -1;
        for (int j = (int)idx - 1; j >= 0; j--) {
            if (tracks[j].depth == level) {
                ancestor_idx = j;
                break;
            }
        }
        if (ancestor_idx >= 0) {
            for (size_t j = ancestor_idx + 1; j < tracks.size(); j++) {
                if (tracks[j].depth < level) break;
                if (tracks[j].depth == level) {
                    ancestor_has_next = true;
                    break;
                }
            }
        }
        prefix += ancestor_has_next ? u8"│   " : u8"    ";
    }

    if (d > 0) {
        bool has_next_sibling = false;
        for (size_t j = idx + 1; j < tracks.size(); j++) {
            if (tracks[j].depth < d) break;
            if (tracks[j].depth == d) {
                has_next_sibling = true;
                break;
            }
        }
        prefix += has_next_sibling ? u8"├── " : u8"└── ";
    }

    float line_h = UI::TREE_LINE_HEIGHT;
    ImVec2 cursor_start = ImGui::GetCursorScreenPos();

    // 整行 Selectable（空标签，仅作交互区域）
    ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns;
    if (read_only || is_empty) {
        flags |= ImGuiSelectableFlags_Disabled;
    }

    bool selected = node.selected;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (ImGui::Selectable(("##sel" + std::to_string(node.number)).c_str(), selected, flags, ImVec2(0, line_h))) {
        if (!read_only && !is_empty) {
            node.selected = !node.selected;
        }
    }
    ImGui::PopStyleVar();

    // 选中指示：最左侧蓝色竖线
    if (node.selected) {
        ImVec2 rmin = ImGui::GetItemRectMin();
        ImVec2 rmax = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(
            ImVec2(rmin.x, rmin.y + 3),
            ImVec2(rmin.x + 3, rmax.y - 3),
            ImGui::GetColorU32(UI::COL_ACCENT),
            1.5f
        );
    }

    // 在同一行渲染文本
    ImGui::SameLine();
    ImVec2 text_pos = ImGui::GetCursorScreenPos();
    float text_y_offset = (line_h - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(text_pos.x, cursor_start.y + text_y_offset));

    // 前缀（分支线，灰色）
    ImGui::PushStyleColor(ImGuiCol_Text, UI::COL_TREE_BRANCH);
    ImGui::TextUnformatted(prefix.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // 编号（粗体）
    std::string num_str = std::to_string(node.number);
    if (g_font_bold && g_font_bold != g_font_normal) {
        ImGui::PushFont(g_font_bold);
    }
    ImGui::TextColored(UI::COL_TRACK_NUM, "%s", num_str.c_str());
    if (g_font_bold && g_font_bold != g_font_normal) {
        ImGui::PopFont();
    }
    ImGui::SameLine();

    // 名称
    std::string name_utf8 = wide_to_utf8(node.name);
    if (name_utf8.empty()) {
        name_utf8 = "Track " + std::to_string(node.number);
    }
    if (is_empty) {
        name_utf8 += u8" (空)";
        ImGui::PushStyleColor(ImGuiCol_Text, UI::COL_TRACK_EMPTY);
    }
    ImGui::TextUnformatted(name_utf8.c_str());
    if (is_empty) {
        ImGui::PopStyleColor();
    }

    ImGui::PopID();
}

void render_track_tree(bool read_only) {
    if (!g_project_state.has_data) {
        ImGui::TextDisabled(u8"未加载音频工程");
        return;
    }

    ImGui::Text(u8"轨道选择");
    ImGui::SameLine();

    if (!read_only) {
        if (GhostSmallButton(u8"全选")) select_all_tracks();
        ImGui::SameLine();
        if (GhostSmallButton(u8"取消")) deselect_all_tracks();
        ImGui::SameLine();
        if (GhostSmallButton(u8"反选")) invert_track_selection();
    } else {
        ImGui::TextDisabled(u8"(只读预览)");
    }

    ImGui::Separator();

    for (size_t i = 0; i < g_project_state.tracks.size(); i++) {
        render_flat_track_node(i, read_only);
    }
}

// ---------------------------------------------------------------------------
// 导航栏
// ---------------------------------------------------------------------------
void render_nav_bar() {
    if (!ImGui::BeginMenuBar()) return;

    // 左侧：文件
    if (ImGui::BeginMenu(u8"文件")) {
        if (ImGui::MenuItem(u8"工程导入页面")) {
            g_current_page = AppPage::Import;
        }
        if (ImGui::MenuItem(u8"配置导入页面")) {
            g_current_page = AppPage::Config;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(u8"首选项...")) {
            // 预留
        }
        ImGui::EndMenu();
    }

    // 右侧：效果链编辑（粗体、高亮背景）
    float bar_width = ImGui::GetWindowWidth();
    float left_x = ImGui::GetCursorPosX();
    float right_x = bar_width - UI::EFFECT_CHAIN_BUTTON_WIDTH - ImGui::GetStyle().FramePadding.x * 2 - 12.0f;
    float space = right_x - left_x;
    if (space > 0) {
        ImGui::SameLine(space);
    }

    if (g_show_effect_editor) {
        ImGui::PushStyleColor(ImGuiCol_Button, UI::COL_ACCENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::COL_ACCENT_HOVER);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::COL_ACCENT_ACTIVE);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, UI::COL_EFFECT_CHAIN_BG);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::COL_EFFECT_CHAIN_HOVER);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::COL_EFFECT_CHAIN_ACTIVE);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, UI::COL_TEXT_PRIMARY);

    if (g_font_bold && g_font_bold != g_font_normal) {
        ImGui::PushFont(g_font_bold);
    }

    if (ImGui::Button(u8"效果链编辑", ImVec2(UI::EFFECT_CHAIN_BUTTON_WIDTH, 0))) {
        g_show_effect_editor = !g_show_effect_editor;
    }

    if (g_font_bold && g_font_bold != g_font_normal) {
        ImGui::PopFont();
    }
    ImGui::PopStyleColor(4);

    ImGui::EndMenuBar();
}

// ---------------------------------------------------------------------------
// 工程导入独立页面
// ---------------------------------------------------------------------------
void render_import_page() {
    ImGui::Text(u8"工程导入");
    ImGui::Separator();

    // --- REAPER 最近文件下拉框 ---
    static std::vector<std::wstring> recent_files;
    static int selected_recent_idx = -1;
    static AppPage last_page = AppPage::Config;

    if (last_page != g_current_page) {
        last_page = g_current_page;
        if (g_current_page == AppPage::Import) {
            recent_files = get_reaper_recent_files_internal();
            selected_recent_idx = -1;
        }
    }

    if (GhostButton(u8"刷新")) {
        recent_files = get_reaper_recent_files_internal();
        selected_recent_idx = -1;
    }
    ImGui::SameLine();

    std::string combo_preview = u8"选择 REAPER 最近文件...";
    if (selected_recent_idx >= 0 && selected_recent_idx < (int)recent_files.size()) {
        std::string name = wide_to_utf8(recent_files[selected_recent_idx]);
        size_t sep = name.find_last_of("\\/");
        combo_preview = (sep != std::string::npos) ? name.substr(sep + 1) : name;
    }

    ImGui::SetNextItemWidth(280);
    if (ImGui::BeginCombo("##RecentCombo", combo_preview.c_str())) {
        for (int i = 0; i < (int)recent_files.size(); i++) {
            std::string name = wide_to_utf8(recent_files[i]);
            bool is_selected = (selected_recent_idx == i);
            if (ImGui::Selectable(name.c_str(), is_selected)) {
                selected_recent_idx = i;
                if (parse_project_file(recent_files[i])) {
                    size_t sep = recent_files[i].find_last_of(L"\\/");
                    if (sep != std::wstring::npos) {
                        save_last_directory(recent_files[i].substr(0, sep));
                    }
                }
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (GhostButton(u8"浏览...")) {
        std::wstring init_dir = load_last_directory();
        show_file_picker(GetActiveWindow(), [](const std::wstring& path) {
            if (parse_project_file(path)) {
                size_t sep = path.find_last_of(L"\\/");
                if (sep != std::wstring::npos) {
                    save_last_directory(path.substr(0, sep));
                }
            }
        }, init_dir);
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text(u8"导入选项");
    ImGui::Separator();
    if (ImGui::InputDouble(u8"基准时间（秒）", &g_project_state.config.base_time_sec, 0.1, 1.0, "%.1f")) {
        update_current_project_offset(g_project_state.config.base_time_sec);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // --- 轨道树（可滚动区域，留空间给底部按钮）---
    float button_height = 45.0f;
    float avail_h = ImGui::GetContentRegionAvail().y - button_height - ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild("ImportTreeScroll", ImVec2(0, std::max(avail_h, 100.0f)), true);
    render_track_tree(true);
    ImGui::EndChild();

    // --- 底部固定确认导入按钮 ---
    if (ImGui::Button(u8"确认导入", ImVec2(140, 35))) {
        if (g_project_state.has_data) {
            if (!g_project_state.template_alias.empty()) {
                if (g_template_aliases.empty()) {
                    g_template_aliases.push_back(g_project_state.template_alias);
                    refresh_template_effects();
                }
                g_current_page = AppPage::Config;
            } else {
                ImGui::OpenPopup(u8"提示");
            }
        } else {
            ImGui::OpenPopup(u8"提示");
        }
    }

    if (ImGui::BeginPopupModal(u8"提示", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!g_project_state.has_data) {
            ImGui::Text(u8"请先选择一个工程文件");
        } else {
            ImGui::Text(u8"工程已导入，但尚未选择模板物件。\n请右键时间轴物件选择配置导入");
        }
        if (ImGui::Button(u8"确定", ImVec2(80, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// 配置导入页面（原主页面）
// ---------------------------------------------------------------------------
static void render_header_panel() {
    std::string preview;
    std::string stats;
    if (g_project_state.has_data) {
        preview = wide_to_utf8(g_project_state.file_name);
        std::wstring summary = get_project_summary();
        std::string summary_utf8 = wide_to_utf8(summary);
        size_t sep = summary_utf8.find(" | ");
        if (sep != std::string::npos) {
            stats = summary_utf8.substr(sep + 3);
        }
    } else {
        preview = u8"未加载音频工程";
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
    if (ImGui::BeginCombo("##ProjectCombo", preview.c_str())) {
        if (ImGui::Selectable(u8"导入新工程...")) {
            g_current_page = AppPage::Import;
        }
        if (!g_project_state.file_history.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled(u8"历史记录");
        }
        for (int i = 0; i < (int)g_project_state.file_history.size(); i++) {
            const std::wstring& path = g_project_state.file_history[i].path;
            std::string name = wide_to_utf8(path);
            size_t sep = name.find_last_of("\\/");
            std::string display = (sep != std::string::npos) ? name.substr(sep + 1) : name;
            bool is_selected = (g_project_state.has_data && g_project_state.file_path == path);

            ImGui::PushID(i);
            if (ImGui::Selectable(display.c_str(), is_selected)) {
                parse_project_file(path);
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem(u8"删除记录")) {
                    remove_file_from_history(i);
                }
                ImGui::EndPopup();
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    if (!stats.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("| %s", stats.c_str());
    } else if (!g_project_state.has_data && !g_project_state.file_history.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled(u8"(从下拉框选择历史工程，或导入新工程)");
    } else if (!g_project_state.has_data) {
        ImGui::SameLine();
        ImGui::TextColored(UI::COL_HINT_TEXT,
            u8"（文件 \u2192 工程导入页面）");
    }
}

void render_config_page() {
    render_header_panel();
    ImGui::Separator();

    float avail_h = ImGui::GetContentRegionAvail().y - 50;
    float avail_w = ImGui::GetContentRegionAvail().x;

    static float anim_width = 0.0f;
    float target_w = g_show_effect_editor ? (avail_w * 0.34f) : 0.0f;
    anim_width = anim_width + (target_w - anim_width) * 0.15f;

    if (anim_width < 1.0f) {
        // === 双列模式 ===
        ImGui::Columns(2, "main_cols", false);
        ImGui::SetColumnWidth(0, avail_w * 0.50f);

        // 左列
        ImGui::BeginChild("ConfigLeft", ImVec2(0, avail_h), true);
        render_track_tree(false);
        ImGui::EndChild();

        ImGui::NextColumn();

        // 右列
        ImGui::BeginChild("ConfigRight", ImVec2(0, avail_h), true);
        render_config_panel();
        ImGui::EndChild();
    } else {
        // === 三列模式 ===
        float remaining = avail_w - anim_width;
        float left_w = remaining * 0.50f;
        float mid_w = remaining - left_w;

        ImGui::Columns(3, "main_cols", false);
        ImGui::SetColumnWidth(0, left_w);
        ImGui::SetColumnWidth(1, mid_w);

        // 左列：轨道树
        ImGui::BeginChild("ConfigLeft", ImVec2(0, avail_h), true);
        render_track_tree(false);
        ImGui::EndChild();

        ImGui::NextColumn();

        // 中列：参数编辑
        ImGui::BeginChild("ConfigMid", ImVec2(0, avail_h), true);
        render_config_panel();
        ImGui::EndChild();

        ImGui::NextColumn();

        // 右列：效果链编辑器
        ImGui::BeginChild("ConfigRight", ImVec2(0, avail_h), true);
        render_effect_chain_panel();
        ImGui::EndChild();
    }

    ImGui::Columns(1);
    render_action_bar();
}

// ---------------------------------------------------------------------------
// 参数设置面板
// ---------------------------------------------------------------------------
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

    bool flip_highlight = (g_highlight_param_id == "flip_config" && g_highlight_timer > 0);
    if (flip_highlight) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, UI::COL_FLIP_FRAME);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, UI::COL_FLIP_FRAME_HOVER);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UI::COL_FLIP_FRAME_ACTIVE);
        ImGui::PushStyleColor(ImGuiCol_Header, UI::COL_FLIP_HEADER);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, UI::COL_FLIP_HEADER_HOVER);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, UI::COL_FLIP_HEADER_ACTIVE);
    }
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

        static const char* counter_modes[] = { u8"全部", u8"图层" };
        ImGui::SetNextItemWidth(100);
        ImGui::Combo(u8"计数模式", &cfg.flip_counter_mode, counter_modes, 2);
        ImGui::Unindent(16);
    }
    if (flip_highlight) {
        ImGui::PopStyleColor(6);
    }
    ImGui::Checkbox(u8"无节拍同步", &cfg.beatless_sync);
    ImGui::Checkbox(u8"向上取整帧", &cfg.use_round_up);

    ImGui::Spacing();
    ImGui::Text(u8"物件与音符同步");
    ImGui::Separator();

    static const char* sync_labels[] = {
        u8"与音符对齐",
        u8"拉伸到下一音符",
        u8"拉伸到固定值",
        u8"仅在间隙生成",
        u8"仅在间隙并拉伸固定值"
    };
    ImGui::Combo(u8"同步模式", &cfg.sync_mode, sync_labels, 5);
    if (cfg.sync_mode == 2 || cfg.sync_mode == 4) {
        ImGui::Indent(16);
        ImGui::InputInt(u8"固定帧数", &cfg.fixed_duration_frames, 1, 10);
        if (cfg.fixed_duration_frames < 1) cfg.fixed_duration_frames = 1;
        ImGui::Unindent(16);
    }

    static const char* layer_strategy_labels[] = { u8"优化模式", u8"持续累加模式", u8"交替换行" };
    ImGui::Combo(u8"层分配策略", &cfg.layer_strategy, layer_strategy_labels, 3);
    ImGui::Checkbox(u8"反转轨道顺序", &cfg.reverse_layer_order);

    static const char* track_filter_labels[] = { u8"全部独立", u8"仅取第N轨", u8"仅取倒数第N轨" };
    ImGui::Combo(u8"多音符策略", &cfg.track_filter_mode, track_filter_labels, 3);
    if (cfg.track_filter_mode != 0) {
        ImGui::Indent(16);
        ImGui::InputInt(u8"N", &cfg.track_filter_n, 1, 1);
        if (cfg.track_filter_n < 1) cfg.track_filter_n = 1;
        ImGui::Unindent(16);
    }

    // 向后兼容：同步旧 no_gap 字段
    cfg.no_gap = (cfg.sync_mode == 1);

    // Phase 4: 多源映射（仅当模板池 >= 2 时显示）
    if (g_template_pool.size() >= 2) {
        ImGui::Spacing();
        ImGui::Text(u8"多模板映射");
        ImGui::Separator();

        ImGui::TextDisabled(u8"当前模板池: %d 个物件（按时间轴顺序）", (int)g_template_pool.size());
        for (size_t i = 0; i < g_template_pool.size(); i++) {
            if (i > 0) ImGui::SameLine();
            ImGui::Text("[%s]", g_template_pool[i].display_name.c_str());
        }

        int strategy_idx = cfg.mapping_strategy - 1;
        if (strategy_idx < 0) strategy_idx = 0;
        if (strategy_idx > 2) strategy_idx = 2;

        ImGui::RadioButton(u8"顺序轮替", &strategy_idx, 0);
        if (strategy_idx == 0) {
            ImGui::Indent(24);
            static const char* order_labels[] = { u8"顺序", u8"倒序", u8"洗牌" };
            ImGui::Combo(u8"##seq_order", &cfg.mapping_sequential_order, order_labels, 3);
            ImGui::Unindent(24);
        }

        ImGui::RadioButton(u8"随机抽选", &strategy_idx, 1);
        if (strategy_idx == 1) {
            ImGui::Indent(24);
            ImGui::Checkbox(u8"禁止连续重复", &cfg.mapping_no_consecutive);
            ImGui::Unindent(24);
        }

        ImGui::RadioButton(u8"和弦映射", &strategy_idx, 2);
        if (strategy_idx == 2) {
            ImGui::Indent(24);
            if (cfg.track_filter_mode != 0) {
                ImGui::TextColored(UI::COL_WARNING_TEXT, u8"⚠ 需要多音符策略设为“全部独立”");
            } else {
                ImGui::TextDisabled(u8"使用和弦位置分配模板");
            }
            ImGui::Unindent(24);
        }

        cfg.mapping_strategy = strategy_idx + 1;
    }
}

// ---------------------------------------------------------------------------
// 底部操作栏
// ---------------------------------------------------------------------------
void render_action_bar() {
    ImGui::Separator();
    if (ImGui::Button(u8"重新选择文件...", ImVec2(140, 0))) {
        imgui_window_hide();
        show_file_picker(GetActiveWindow(), [](const std::wstring& path) {
            if (parse_project_file(path)) {
                imgui_window_show();
            }
        });
    }

    float right_x = ImGui::GetContentRegionAvail().x - 110 - 110 - ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine(right_x);

    if (ImGui::Button(u8"应用", ImVec2(110, 30))) {
        if (g_project_state.has_data)
            imgui_window_trigger_generate();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"确定", ImVec2(110, 30))) {
        if (g_project_state.has_data) {
            imgui_window_trigger_generate();
            imgui_window_hide();
        }
    }
}
