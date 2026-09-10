#include "plugin.h"
#include "i18n/i18n.h"
#include "config2.h"
#include "core/app_message.h"
#include "parsers/rpp/rpp_parser.h"
#include "parsers/midi/midi_parser.h"
#include "effect/effect_dict.h"
#include "ui/file_picker.h"
#include "ui/imgui_window.h"
#include "ui/effect_chain_editor.h"
#include "ui/font_support.h"
#include "script/expr_evaluator.h"
#include "script/variable_subst.h"
#include "chain/template_chain.h"
#include "generation/generation.h"
#include "codec/codec.h"
#include "adapters/menu_registry.h"
#include "core/preferences.h"
#include "tools/up/up.h"
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
    L"AutoZWJ - RPP/MIDI/UTAU to AviUtl2 object importer",
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

static bool text_is_ascii(LPCWSTR text) {
    for (const wchar_t* p = text; *p; p++) {
        if (*p > 0x7F) return false;
    }
    return true;
}

// 宿主界面语言：以语言文件中确定存在的词条为探针。
// 返回值与键相同表示未翻译（宿主使用日文默认语言），译文含简体专用字形表示简体中文，
// 纯 ASCII 译文表示英文，其余（繁体/韩文等）沿用既有行为按日文处理。
static Lang detect_aviutl2_lang() {
    if (!g_config_handle) return Lang::ZhCN;
    static constexpr struct { LPCWSTR section; LPCWSTR key; } kProbes[] = {
        { L"Effect", L"動画ファイル" },
        { L"Effect", L"音声ファイル" },
        { L"Effect", L"テキスト" },
    };
    bool english = false;
    for (const auto& probe : kProbes) {
        LPCWSTR text = g_config_handle->get_language_text(g_config_handle, probe.section, probe.key);
        if (!text || !text[0]) return Lang::ZhCN;
        if (wcscmp(text, probe.key) == 0) continue;
        if (text_has_simplified_only_glyphs(text)) return Lang::ZhCN;
        if (text_is_ascii(text)) {
            english = true;
            continue;
        }
        return Lang::Ja;
    }
    return english ? Lang::En : Lang::Ja;
}

// 首次启动时的界面字体校正：AviUtl2 默认字体（Yu Gothic UI）缺少简体字形，
// 中文界面会显示为问号，且多数字体的中文字形默认优先日文写法，
// 因此简体中文宿主下自动分配简体中文字体（微软雅黑等）。
static void apply_first_run_chinese_ui_font() {
    if (preferences().font_auto_setup_done) return;
    preferences().font_auto_setup_done = true;

    if (detect_aviutl2_lang() == Lang::ZhCN) {
        std::wstring current = preferences().font_name;
        if (current.empty()) host_get_default_font(current);  // 跟随主题时按主题字体判断
        if (!font_natively_supports_chinese_ui(current)) {
            const std::wstring assigned = pick_simplified_chinese_ui_font();
            if (!assigned.empty()) {
                preferences().font_name = assigned;
                const std::wstring from = current.empty() ? std::wstring(L"(theme)") : current;
                if (g_host.logger)
                    g_host.logger->log(g_host.logger,
                        (L"AutoZWJ: UI font auto-assigned: " + from + L" -> " + assigned).c_str());
            } else if (g_host.logger) {
                g_host.logger->warn(g_host.logger,
                    L"AutoZWJ: no Simplified Chinese UI font found, keep current UI font");
            }
        }
    }
    preferences_save();
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
    if (config && config->app_data_path) {
        menu_registry_load(config->app_data_path);
        preferences_load(config->app_data_path);
        apply_first_run_chinese_ui_font();
    }
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
    bool any_created = false;
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
        auto obj = edit->create_object_from_alias(s.alias_chain.c_str(), s.layer, s.sf - 1, s.ef - s.sf);
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
                    if (s.name_sub_index > 0) {
                        nm += L"." + std::to_wstring(s.name_sub_index);
                    }
                    break;
            }
            edit->set_object_name(obj, nm.c_str());
        }
    }

    for (const auto& w : result.warnings) {
        if (g_host.logger)
            g_host.logger->warn(g_host.logger, utf8_to_wide(w).c_str());
    }

    cs->any_created = !result.objects.empty();
    if (cs->any_created) {
        if (g_host.logger)
            g_host.logger->log(g_host.logger, utf8_to_wide(tr_fmt(u8"已生成 {} 个物件", result.objects.size())).c_str());
        app_msg_set(AppMsgSeverity::Success,
            tr_fmt(u8"已生成 {} 个物件", result.objects.size()));
    } else {
        if (g_host.logger)
            g_host.logger->warn(g_host.logger, utf8_to_wide(tr_str(u8"未生成任何物件，请检查轨道选择")).c_str());
        app_msg_set(AppMsgSeverity::Error, tr_str(u8"未生成任何物件，请检查轨道选择"));
    }
}

static bool on_generate_from_imgui() {
    if (!g_app.project.has_data) {
        app_msg_set(AppMsgSeverity::Error, tr_str(u8"未加载音频工程"));
        if (g_host.logger) g_host.logger->warn(g_host.logger, utf8_to_wide(tr_str(u8"未加载音频工程")).c_str());
        return false;
    }
    if (g_app.templates.empty()) {
        app_msg_set(AppMsgSeverity::Error, tr_str(u8"未设置模板，请右键已选物件 → 配置导入... 重新打开"));
        if (g_host.logger) g_host.logger->error(g_host.logger, utf8_to_wide(tr_str(u8"未设置模板，请右键已选物件 → 配置导入... 重新打开")).c_str());
        return false;
    }

    save_current_template_data();

    uint32_t seed = 0;
    if (!g_app.project.file_path.empty()) {
        seed = hash_string(wide_to_utf8(g_app.project.file_path));
    }

    auto* cs = new GenCallState{seed};
    g_host.edit_handle->call_edit_section_param(cs, gen_edit_callback);
    bool ok = cs->any_created;
    delete cs;
    return ok;
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
        // 无模板也打开面板：无工程先进导入页，有工程进配置页由横幅引导
        if (g_app.project.has_data) {
            imgui_window_show();
        } else {
            imgui_window_show_import_page();
        }
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
        imgui_window_show();
    } else {
        // 解析失败且窗口未打开：打开窗口展示结果栏错误，避免拖放无任何反馈
        // （EDIT_SECTION 回调上下文不使用阻塞式 MessageBox 防止宿主重入）
        if (g_host.logger)
            g_host.logger->warn(g_host.logger, utf8_to_wide(app_msg_current().text).c_str());
        if (!imgui_window_is_visible()) {
            imgui_window_show();
        }
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

bool host_get_default_font(std::wstring& name) {
    if (!g_config_handle || !g_config_handle->get_font_info) return false;
    FONT_INFO* info = g_config_handle->get_font_info(g_config_handle, "Default");
    if (!info || !info->name || !info->name[0]) return false;
    name = info->name;
    return true;
}

static void on_open_preferences(HWND, HINSTANCE) {
    imgui_window_show_preferences();
}

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    HMODULE dll_hinst = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&RegisterPlugin),
        &dll_hinst);
    g_host.dll_hinst = dll_hinst;

    static std::wstring s_menu_config = utf8_to_wide(tr_str(u8"配置导入..."));
    // AviUtl2 expects a Windows file-dialog filter: description, NUL, patterns, NUL, NUL.
    static constexpr wchar_t k_file_drop_filter[] =
        L"RPP/MIDI/UTAU Files\0*.rpp;*.mid;*.midi;*.ust;*.ustx\0";
    host->register_object_menu(s_menu_config.c_str(), on_open_config);
    static std::wstring s_menu_preferences = utf8_to_wide(tr_str(u8"首选项..."));
    host->register_config_menu(s_menu_preferences.c_str(), on_open_preferences);
    host->register_file_drop_handler(L"[AutoZWJ] RPP/MIDI/UTAU Input", k_file_drop_filter, on_file_drop);
    up_register_menu(host);

    g_host.edit_handle = host->create_edit_handle();

    HWND host_wnd = g_host.edit_handle->get_host_app_window();
    imgui_window_init(g_host.dll_hinst, host_wnd);
    imgui_window_set_generate_callback(on_generate_from_imgui);

    if (g_host.logger) g_host.logger->log(g_host.logger, L"AutoZWJ plugin registered");
}
