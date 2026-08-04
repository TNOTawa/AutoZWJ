#include "plugin.h"
#include "i18n/i18n.h"
#include "config2.h"
#include "parsers/rpp/rpp_parser.h"
#include "parsers/midi/midi_parser.h"
#include "exo/object_generator.h"
#include "effect/effect_dict.h"
#include "ui/file_picker.h"
#include "ui/imgui_window.h"
#include "ui/effect_chain_editor.h"
#include "script/expr_evaluator.h"
#include "script/variable_subst.h"
#include "chain/template_chain.h"
#include "generation/generation.h"
#include "codec/codec.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <cstdint>
#include <format>
#include <unordered_set>

AppState g_app;
HostContext g_host;

static uint32_t hash_string(const std::string& s) {
    uint32_t h = 5381;
    for (char c : s) {
        h = ((h << 5) + h) + static_cast<unsigned char>(c);
    }
    return h;
}

COMMON_PLUGIN_TABLE common_plugin_table = {
    L"AutoZWJ",
    L"AutoZWJ - RPP/MIDI to AviUtl2 object importer",
};

EXTERN_C __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable(void) {
    return &common_plugin_table;
}

EXTERN_C __declspec(dllexport) DWORD RequiredVersion() {
    return 2003300;
}

EXTERN_C __declspec(dllexport) void InitializeLogger(LOG_HANDLE* handle) {
    g_host.logger = handle;
}

static CONFIG_HANDLE* g_config_handle = nullptr;
static std::vector<std::wstring> g_plugin_sections;

static void scan_plugin_language_sections() {
    g_plugin_sections.clear();
    if (!g_config_handle || !g_config_handle->app_data_path) return;

    std::wstring lang_dir = std::wstring(g_config_handle->app_data_path) + L"\\Language\\*.aul2";

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(lang_dir.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    std::unordered_set<std::wstring> seen;
    do {
        std::wstring fname(fd.cFileName);
        size_t dot = fname.find(L'.');
        if (dot == std::wstring::npos || dot == 0) continue;
        std::wstring section = fname.substr(dot + 1);
        size_t ext = section.rfind(L".aul2");
        if (ext == std::wstring::npos) continue;
        section = section.substr(0, ext);
        if (section.empty() || section == L"Effect") continue;
        if (seen.insert(section).second) {
            g_plugin_sections.push_back(std::move(section));
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

static Lang detect_aviutl2_lang() {
    if (!g_config_handle) return Lang::ZhCN;
    LPCWSTR r = g_config_handle->get_language_text(g_config_handle, L"Effect", L"動画ファイル");
    if (!r) return Lang::ZhCN;
    if (wcscmp(r, L"视频文件") == 0) return Lang::ZhCN;
    if (wcscmp(r, L"VideoFile") == 0) return Lang::En;
    return Lang::Ja;
}

std::string host_translate_effect_name(const std::string& ja_name) {
    if (!g_config_handle || ja_name.empty()) return ja_name;
    std::wstring w_key = utf8_to_wide(ja_name);

    LPCWSTR result = g_config_handle->get_language_text(g_config_handle, L"Effect", w_key.c_str());
    if (result && wcscmp(result, w_key.c_str()) != 0) return wide_to_utf8(result);

    for (const auto& section : g_plugin_sections) {
        result = g_config_handle->get_language_text(g_config_handle, section.c_str(), w_key.c_str());
        if (result && wcscmp(result, w_key.c_str()) != 0) return wide_to_utf8(result);
    }
    return ja_name;
}

EXTERN_C __declspec(dllexport) void InitializeConfig(CONFIG_HANDLE* config) {
    g_config_handle = config;
    scan_plugin_language_sections();
    i18n_set_host_lang_detector(detect_aviutl2_lang);
}

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) {
    (void)version;
    i18n_init();
    return true;
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {
    imgui_window_shutdown();
}

static void update_scene_from_edit(EDIT_SECTION* edit) {
    if (!edit || !edit->info) return;
    auto& info = *edit->info;
    g_app.scene.width = info.width;
    g_app.scene.height = info.height;
    g_app.scene.rate = info.rate;
    g_app.scene.scale = info.scale;
    g_app.scene.sample_rate = info.sample_rate;
    g_app.scene.frame = info.frame;
    g_app.scene.layer = info.layer;
    g_app.scene.valid = true;
    g_app.project.config.fps_num = info.rate;
    g_app.project.config.fps_den = info.scale;
}

struct GenCallState {
    uint32_t seed;
};

static void gen_edit_callback(void* param, EDIT_SECTION* edit) {
    auto* cs = static_cast<GenCallState*>(param);
    update_scene_from_edit(edit);

    GenerationInput in{
        g_app.project.objdict,
        g_app.project.tracks,
        g_app.project.config,
        g_app.templates,
        g_app.scene,
        g_app.project.template_layer + 1,
        cs->seed
    };

    auto result = generate(in);

    for (auto& s : result.objects) {
        auto obj = edit->create_object_from_alias(s.alias_chain.c_str(), s.layer, s.sf, s.ef - s.sf);
        if (obj) {
            std::wstring nm;
            switch (s.name_kind) {
                case ObjNameKind::FilePath:
                    nm = utf8_to_wide(s.name_text);
                    break;
                case ObjNameKind::Gap:
                    nm = utf8_to_wide(tr_str(u8"Gap")) + L" " + std::to_wstring(s.name_index);
                    break;
                case ObjNameKind::Item:
                    nm = utf8_to_wide(tr_str(u8"Item")) + L" " + std::to_wstring(s.name_index);
                    break;
            }
            edit->set_object_name(obj, nm.c_str());
        }
    }

    for (const auto& w : result.warnings) {
        if (g_host.logger)
            g_host.logger->warn(g_host.logger, utf8_to_wide(w).c_str());
    }

    if (g_host.logger)
        g_host.logger->log(g_host.logger, utf8_to_wide(tr_fmt(u8"已生成 {} 个物件", result.objects.size())).c_str());

    delete cs;
}

static void on_generate_from_imgui() {
    if (!g_app.project.has_data) {
        if (g_host.logger) g_host.logger->warn(g_host.logger, utf8_to_wide(tr_str(u8"未加载音频工程")).c_str());
        return;
    }
    if (g_app.templates.empty()) {
        if (g_host.logger) g_host.logger->error(g_host.logger, utf8_to_wide(tr_str(u8"未设置模板，请右键已选物件 → 配置导入... 重新打开")).c_str());
        return;
    }

    save_current_template_data();

    uint32_t seed = 0;
    if (!g_app.project.file_path.empty()) {
        seed = hash_string(wide_to_utf8(g_app.project.file_path));
    }

    auto* cs = new GenCallState{seed};
    g_host.edit_handle->call_edit_section_param(cs, gen_edit_callback);
}

static void on_select_project(EDIT_SECTION* edit) {
    update_scene_from_edit(edit);
    load_project_state_from_project_file(edit, g_app, g_host);
    flush_project_file_state(edit, g_app, g_host);
    imgui_window_show_import_page();
}

static void on_open_config(EDIT_SECTION* edit) {
    update_scene_from_edit(edit);
    load_project_state_from_project_file(edit, g_app, g_host);

    if (!g_app.project.has_data) {
        std::wstring path_to_load;
        if (!g_app.project.file_path.empty()) {
            path_to_load = g_app.project.file_path;
        } else if (!g_app.project.file_history.empty()) {
            path_to_load = g_app.project.file_history[0].path;
        }
        if (!path_to_load.empty()) {
            select_project(g_app, path_to_load);
        }
    }

    flush_project_file_state(edit, g_app, g_host);

    int sel_num = edit->get_selected_object_num();
    if (sel_num <= 0) {
        if (g_host.logger) g_host.logger->error(g_host.logger, utf8_to_wide(tr_str(u8"请先在时间轴上选择一个物件作为模板，再右键打开配置")).c_str());
        return;
    }

    g_app.templates.clear();

    for (int i = 0; i < sel_num; i++) {
        auto obj = edit->get_selected_object(i);
        if (!obj) continue;
        LPCSTR alias_ptr = edit->get_object_alias(obj);
        if (!alias_ptr) continue;

        TemplateEntry entry;
        entry.object_id = i;
        entry.alias = alias_ptr;
        entry.chain = extract_template_chain(entry.alias);
        if (entry.chain.empty()) continue;

        auto lf = edit->get_object_layer_frame(obj);
        entry.layer = lf.layer;
        entry.sf = static_cast<double>(lf.start);
        entry.ef = lf.end;

        LPCWSTR name_ptr = edit->get_object_name(obj);
        if (name_ptr && name_ptr[0]) {
            entry.display_name = wide_to_utf8(name_ptr);
        } else {
            entry.display_name = tr_str(u8"模板") + std::to_string(i + 1);
        }

        g_app.templates.push_back({std::move(entry), {}, {}, {}});
    }

    if (g_app.templates.empty()) {
        if (g_host.logger) g_host.logger->error(g_host.logger, utf8_to_wide(tr_str(u8"无法读取任何选中物件的数据")).c_str());
        return;
    }

    std::sort(g_app.templates.begin(), g_app.templates.end(), [](const TemplateSession& a, const TemplateSession& b) {
        if (a.source.layer != b.source.layer) return a.source.layer < b.source.layer;
        return a.source.sf < b.source.sf;
    });

    g_app.project.template_alias = g_app.templates[0].source.alias;
    g_app.project.template_layer = g_app.templates[0].source.layer;
    g_app.current_template_index = 0;

    // 同步模式"拉伸到固定值"的默认帧数取主模板物件本身的时长（帧）。
    // SDK 的 start/end 均为 0-based 且 end 为末帧（含），故帧数 = end - start + 1；
    // 取值异常（<=0）时不修改，保持 OutputConfig 初值 30 作为兜底。
    {
        const auto& primary = g_app.templates[0].source;
        int tpl_duration = primary.ef - static_cast<int>(primary.sf) + 1;
        if (tpl_duration > 0) {
            g_app.project.config.fixed_duration_frames = tpl_duration;
        }
    }

    g_param_bakes.clear();
    g_presets.clear();
    g_template_effects.clear();
    g_template_effects_dirty = true;
    refresh_template_effects();
    sync_presets_from_config();

    if (!g_app.project.has_data) {
        imgui_window_show_import_page();
        return;
    }
    imgui_window_show();
}

static void on_file_drop(EDIT_SECTION* edit, LPCWSTR file) {
    update_scene_from_edit(edit);
    load_project_state_from_project_file(edit, g_app, g_host);
    if (select_project(g_app, file)) {
        flush_project_file_state(edit, g_app, g_host);
        if (g_host.logger)
            g_host.logger->log(g_host.logger, (L"AutoZWJ: " + get_project_summary(g_app)).c_str());
    }
}

void sync_scene_info() {
    if (!g_host.edit_handle) return;
    EDIT_INFO info = {};
    g_host.edit_handle->get_edit_info(&info, sizeof(info));
    g_app.scene.width = info.width;
    g_app.scene.height = info.height;
    g_app.scene.rate = info.rate;
    g_app.scene.scale = info.scale;
    g_app.scene.sample_rate = info.sample_rate;
    g_app.scene.frame = info.frame;
    g_app.scene.layer = info.layer;
    g_app.scene.valid = true;
    g_app.project.config.fps_num = info.rate;
    g_app.project.config.fps_den = info.scale;
}

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    g_host.dll_hinst = GetModuleHandle(nullptr);

    static std::wstring s_menu_select = utf8_to_wide(tr_str(u8"选择音频工程..."));
    static std::wstring s_menu_config = utf8_to_wide(tr_str(u8"配置导入..."));
    host->register_layer_menu(s_menu_select.c_str(), on_select_project);
    host->register_object_menu(s_menu_config.c_str(), on_open_config);
    host->register_file_drop_handler(L"[AutoZWJ] RPP/MIDI Input", L"*.rpp;*.mid", on_file_drop);

    g_host.edit_handle = host->create_edit_handle();

    HWND host_wnd = g_host.edit_handle->get_host_app_window();
    imgui_window_init(g_host.dll_hinst, host_wnd);
    imgui_window_set_generate_callback(on_generate_from_imgui);

    if (g_host.logger) g_host.logger->log(g_host.logger, L"AutoZWJ plugin registered");
}
