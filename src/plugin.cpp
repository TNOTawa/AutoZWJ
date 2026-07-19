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

ProjectState g_project_state;
SceneInfo g_scene_info;
EDIT_HANDLE* g_edit_handle = nullptr;
LOG_HANDLE* g_logger = nullptr;
HINSTANCE g_dll_hinst = nullptr;
std::vector<TemplateEntry> g_template_pool;

static uint32_t hash_string(const std::string& s) {
    uint32_t h = 5381;
    for (char c : s) {
        h = ((h << 5) + h) + static_cast<unsigned char>(c);
    }
    return h;
}

static std::vector<int> generate_shuffled_order(int pool_size, uint32_t seed) {
    std::vector<int> order(pool_size);
    for (int i = 0; i < pool_size; i++) order[i] = i;
    for (int i = pool_size - 1; i > 0; i--) {
        seed = seed * 1103515245u + 12345u;
        int j = static_cast<int>(seed % static_cast<uint32_t>(i + 1));
        std::swap(order[i], order[j]);
    }
    return order;
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
    g_logger = handle;
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
    i18n_init();
    return true;
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {
    imgui_window_shutdown();
}

static void update_scene_from_edit(EDIT_SECTION* edit) {
    if (!edit || !edit->info) return;
    auto& info = *edit->info;
    g_scene_info.width = info.width;
    g_scene_info.height = info.height;
    g_scene_info.rate = info.rate;
    g_scene_info.scale = info.scale;
    g_scene_info.sample_rate = info.sample_rate;
    g_scene_info.frame = info.frame;
    g_scene_info.layer = info.layer;
    g_scene_info.valid = true;
    g_project_state.config.fps_num = info.rate;
    g_project_state.config.fps_den = info.scale;
}

struct GenCallState {
    std::vector<int> shuffled_order;
    uint32_t seed_base;
};

static void gen_edit_callback(void* param, EDIT_SECTION* edit) {
    auto* cs = static_cast<GenCallState*>(param);
    update_scene_from_edit(edit);

    GenInput in;
    in.objdict = &g_project_state.objdict;
    in.tracks = &g_project_state.tracks;
    in.config = &g_project_state.config;
    in.template_pool = g_template_pool;
    in.bakes_per_tpl = &g_param_bakes_per_tpl;
    in.presets_per_tpl = &g_template_presets_per_tpl;
    in.scene = g_scene_info;
    in.base_layer = g_project_state.template_layer + 1;
    in.seed_base = cs->seed_base;
    in.shuffled_order = std::move(cs->shuffled_order);
    in.logger = g_logger;

    auto specs = generate_object_specs(in);

    for (auto& s : specs) {
        auto obj = edit->create_object_from_alias(s.alias_chain.c_str(), s.layer, s.sf, s.ef - s.sf);
        if (obj) edit->set_object_name(obj, s.name.c_str());
    }

    if (g_logger)
        g_logger->log(g_logger, utf8_to_wide(tr_fmt(u8"已生成 {} 个物件", specs.size())).c_str());

    delete cs;
}

static void on_generate_from_imgui() {
    if (!g_project_state.has_data) {
        if (g_logger) g_logger->warn(g_logger, utf8_to_wide(tr_str(u8"未加载音频工程")).c_str());
        return;
    }
    if (g_template_pool.empty()) {
        if (g_logger) g_logger->error(g_logger, utf8_to_wide(tr_str(u8"未设置模板，请右键已选物件 → 配置导入... 重新打开")).c_str());
        return;
    }

    save_current_template_data();

    uint32_t seed_base = 0;
    if (!g_project_state.file_path.empty()) {
        seed_base = hash_string(wide_to_utf8(g_project_state.file_path));
    }

    std::vector<int> shuffled_order;
    if (g_project_state.config.mapping_strategy == 1 &&
        g_project_state.config.mapping_sequential_order == 2 &&
        g_template_pool.size() > 1) {
        shuffled_order = generate_shuffled_order(static_cast<int>(g_template_pool.size()), seed_base);
    }

    auto* cs = new GenCallState{std::move(shuffled_order), seed_base};
    g_edit_handle->call_edit_section_param(cs, gen_edit_callback);
}

static void on_select_project(EDIT_SECTION* edit) {
    update_scene_from_edit(edit);
    load_project_state_from_project_file(edit);
    flush_project_file_state(edit);
    imgui_window_show_import_page();
}

static void on_open_config(EDIT_SECTION* edit) {
    update_scene_from_edit(edit);
    load_project_state_from_project_file(edit);

    if (!g_project_state.has_data) {
        std::wstring path_to_load;
        if (!g_project_state.file_path.empty()) {
            path_to_load = g_project_state.file_path;
        } else if (!g_project_state.file_history.empty()) {
            path_to_load = g_project_state.file_history[0].path;
        }
        if (!path_to_load.empty()) {
            parse_project_file(path_to_load);
        }
    }

    flush_project_file_state(edit);

    int sel_num = edit->get_selected_object_num();
    if (sel_num <= 0) {
        if (g_logger) g_logger->error(g_logger, utf8_to_wide(tr_str(u8"请先在时间轴上选择一个物件作为模板，再右键打开配置")).c_str());
        return;
    }

    g_template_pool.clear();
    g_template_aliases.clear();

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

        LPCWSTR name_ptr = edit->get_object_name(obj);
        if (name_ptr && name_ptr[0]) {
            entry.display_name = wide_to_utf8(name_ptr);
        } else {
            entry.display_name = tr_str(u8"模板") + std::to_string(i + 1);
        }

        g_template_pool.push_back(entry);
        g_template_aliases.push_back(entry.alias);
    }

    if (g_template_pool.empty()) {
        if (g_logger) g_logger->error(g_logger, utf8_to_wide(tr_str(u8"无法读取任何选中物件的数据")).c_str());
        return;
    }

    std::sort(g_template_pool.begin(), g_template_pool.end(), [](const TemplateEntry& a, const TemplateEntry& b) {
        if (a.layer != b.layer) return a.layer < b.layer;
        return a.sf < b.sf;
    });

    g_template_aliases.clear();
    for (auto& tpl : g_template_pool) {
        g_template_aliases.push_back(tpl.alias);
    }

    g_project_state.template_alias = g_template_pool[0].alias;
    g_project_state.template_layer = g_template_pool[0].layer;
    g_current_template_idx = 0;

    int pool_size = static_cast<int>(g_template_pool.size());
    g_template_effects_per_tpl.assign(pool_size, {});
    g_param_bakes_per_tpl.assign(pool_size, {});
    g_template_presets_per_tpl.assign(pool_size, {});

    g_param_bakes.clear();
    g_presets.clear();
    g_template_effects.clear();
    g_template_effects_dirty = true;
    refresh_template_effects();
    sync_presets_from_config();

    if (!g_project_state.has_data) {
        imgui_window_show_import_page();
        return;
    }
    imgui_window_show();
}

static void on_file_drop(EDIT_SECTION* edit, LPCWSTR file) {
    update_scene_from_edit(edit);
    load_project_state_from_project_file(edit);
    if (parse_project_file(file)) {
        flush_project_file_state(edit);
        if (g_logger)
            g_logger->log(g_logger, (L"AutoZWJ: " + get_project_summary()).c_str());
    }
}

void sync_scene_info() {
    if (!g_edit_handle) return;
    EDIT_INFO info = {};
    g_edit_handle->get_edit_info(&info, sizeof(info));
    g_scene_info.width = info.width;
    g_scene_info.height = info.height;
    g_scene_info.rate = info.rate;
    g_scene_info.scale = info.scale;
    g_scene_info.sample_rate = info.sample_rate;
    g_scene_info.frame = info.frame;
    g_scene_info.layer = info.layer;
    g_scene_info.valid = true;
    g_project_state.config.fps_num = info.rate;
    g_project_state.config.fps_den = info.scale;
}

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    g_dll_hinst = GetModuleHandle(nullptr);

    static std::wstring s_menu_select = utf8_to_wide(tr_str(u8"选择音频工程..."));
    static std::wstring s_menu_config = utf8_to_wide(tr_str(u8"配置导入..."));
    host->register_layer_menu(s_menu_select.c_str(), on_select_project);
    host->register_object_menu(s_menu_config.c_str(), on_open_config);
    host->register_file_drop_handler(L"[AutoZWJ] RPP/MIDI Input", L"*.rpp;*.mid", on_file_drop);

    g_edit_handle = host->create_edit_handle();

    HWND host_wnd = g_edit_handle->get_host_app_window();
    imgui_window_init(g_dll_hinst, host_wnd);
    imgui_window_set_generate_callback(on_generate_from_imgui);

    if (g_logger) g_logger->log(g_logger, L"AutoZWJ plugin registered");
}
