#include "plugin.h"
#include "rpp/rpp_parser.h"
#include "midi/midi_parser.h"
#include "exo/object_generator.h"
#include "effect/effect_dict.h"
#include "ui/file_picker.h"
#include "ui/imgui_window.h"
#include <algorithm>
#include <sstream>
#include <cmath>

ProjectState g_project_state;
SceneInfo g_scene_info;
EDIT_HANDLE* g_edit_handle = nullptr;
LOG_HANDLE* g_logger = nullptr;
HINSTANCE g_dll_hinst = nullptr;

std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

std::string cp932_to_utf8(const std::string& cp932) {
    if (cp932.empty()) return "";
    int wide_len = MultiByteToWideChar(932, 0, cp932.c_str(), -1, nullptr, 0);
    if (wide_len <= 0) return cp932;
    std::wstring wide(wide_len - 1, L'\0');
    MultiByteToWideChar(932, 0, cp932.c_str(), -1, &wide[0], wide_len);
    return wide_to_utf8(wide);
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

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) {
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

static std::string extract_template_chain(const std::string& alias) {
    size_t pos = alias.find("\n[Object.0]");
    if (pos != std::string::npos) { pos++; }
    else {
        pos = alias.find("\n[0.0]");
        if (pos != std::string::npos) { pos++; }
        else if (alias.find("[Object.0]") == 0) { pos = 0; }
        else if (alias.find("[0.0]") == 0) { pos = 0; }
        else { return ""; }
    }

    std::string chain = alias.substr(pos);

    size_t rp = 0;
    while ((rp = chain.find("[Object.", rp)) != std::string::npos) {
        chain.replace(rp, 8, "[0.");
        rp += 3;
    }

    return chain;
}

static std::string apply_template_to_item(const std::string& template_chain,
                                            double soffs, double playrate,
                                            const std::string& file_path,
                                            int loop) {
    std::string result = template_chain;
    size_t pos;

    pos = 0;
    while ((pos = result.find(u8"再生位置=", pos)) != std::string::npos) {
        size_t end = result.find('\n', pos);
        if (end == std::string::npos) end = result.size();
        result.replace(pos, end - pos, u8"再生位置=" + std::to_string((int)soffs));
        pos++;
    }

    pos = 0;
    while ((pos = result.find(u8"再生速度=", pos)) != std::string::npos) {
        size_t end = result.find('\n', pos);
        if (end == std::string::npos) end = result.size();
        result.replace(pos, end - pos, u8"再生速度=" + std::to_string((int)(playrate * 100.0)));
        pos++;
    }

    pos = 0;
    if (!file_path.empty()) {
        while ((pos = result.find(u8"ファイル=", pos)) != std::string::npos) {
            size_t end = result.find('\n', pos);
            if (end == std::string::npos) end = result.size();
            result.replace(pos, end - pos, u8"ファイル=" + file_path);
            pos++;
        }
    }

    pos = 0;
    while ((pos = result.find(u8"ループ再生=", pos)) != std::string::npos) {
        size_t end = result.find('\n', pos);
        if (end == std::string::npos) end = result.size();
        result.replace(pos, end - pos, u8"ループ再生=" + std::to_string(loop));
        pos++;
    }

    return result;
}

static void on_generate_from_imgui() {
    if (!g_project_state.has_data) {
        if (g_logger) g_logger->warn(g_logger, L"AutoZWJ: 未加载音频工程");
        return;
    }
    if (g_project_state.template_alias.empty()) {
        if (g_logger) g_logger->error(g_logger, L"AutoZWJ: 未设置模板，请右键已选物件 → 配置导入... 重新打开");
        return;
    }

    struct GenState {
        ObjDict* objdict;
        std::vector<TrackNode>* tracks;
        OutputConfig* config;
        std::string template_chain;
        LOG_HANDLE* logger;
        int created;
        int base_layer;
    };

    std::string chain = extract_template_chain(g_project_state.template_alias);
    if (chain.empty()) {
        if (g_logger) g_logger->error(g_logger, L"AutoZWJ: 模板物件格式无效，请重新选择模板");
        return;
    }

    auto* state = new GenState{ &g_project_state.objdict, &g_project_state.tracks,
                                 &g_project_state.config, chain, g_logger, 0,
                                 g_project_state.template_layer + 1 };

    g_edit_handle->call_edit_section_param(state, [](void* param, EDIT_SECTION* edit) {
        auto* gs = static_cast<GenState*>(param);
        update_scene_from_edit(edit);

        auto& objdict = *gs->objdict;
        auto& tracks = *gs->tracks;
        auto& config = *gs->config;
        int base = gs->base_layer;

        double fps = (double)config.fps_num / (double)config.fps_den;
        size_t item_start = 1;
        std::vector<double> opt_layer, opt_layer2;

        int bfidx_global = 0;
        int item_count_global = 0;
        int altidx_global = 0;
        int bpos_global = 0;
        double prev_pitch = -999.0;

        for (size_t ti = 0; ti < tracks.size(); ti++) {
            if (!tracks[ti].selected) {
                while (item_start < objdict.pos.size() && objdict.pos[item_start] != -1.0) item_start++;
                if (item_start < objdict.pos.size()) item_start++;
                continue;
            }

            size_t track_end = objdict.pos.size();
            for (size_t j = item_start + 1; j < objdict.pos.size(); j++)
                if (objdict.pos[j] == -1.0) { track_end = j; break; }

            int prev_count = item_count_global;

            for (size_t i = item_start; i < track_end; i++) {
                double pos_sec = objdict.pos[i];
                double len_sec = objdict.length[i];
                double pitch = objdict.pitch[i];

                if (pos_sec < -0.5) {
                    bfidx_global = -item_count_global;
                    base += (int)(opt_layer.size() + opt_layer2.size());
                    opt_layer.clear();
                    opt_layer2.clear();
                    continue;
                }

                int loop = objdict.loop[i];
                double playrate = objdict.playrate[i];
                double soffs = objdict.soffs[i];
                int fileidx = objdict.fileidx[i];

                double obj_fp = pos_sec * fps + 1.0;
                double obj_fl = len_sec * fps;

                double next_fp = (i + 1 < track_end && objdict.pos[i + 1] > -0.5)
                    ? objdict.pos[i + 1] * fps + 1.0 : -1;

                if (std::round(obj_fp + obj_fl) == std::round(next_fp) - 1)
                    obj_fl += 1;

                if (obj_fp < 0) continue;

                double bf = obj_fp + obj_fl - 1;

                if (config.no_gap && next_fp > 0) {
                    double rounded_bf = config.use_round_up ? std::ceil(bf) : std::round(bf);
                    double rounded_next = config.use_round_up ? std::ceil(next_fp) : std::round(next_fp);
                    if (rounded_bf < rounded_next - 1) {
                        bf = next_fp - 1;
                    }
                }

                auto sur_round = [&](double v) -> double {
                    return config.use_round_up ? std::ceil(v) : std::round(v);
                };

                int sf = (int)sur_round(obj_fp);
                if (sf < 1) sf = 1;
                int ef = (int)sur_round(bf);
                if (ef <= sf) ef = sf + 1;

                bool same_pitch = (std::abs(pitch - prev_pitch) < 0.001);
                bool is_pitch_repeated = (prev_pitch > -999.0 && same_pitch);
                prev_pitch = pitch;

                if (sf == bpos_global)
                    bfidx_global--;
                else if (config.alt_flip && is_pitch_repeated)
                    bfidx_global--;

                if (config.alt_flip && is_pitch_repeated)
                    altidx_global++;
                else if (config.flip_type != FLIP_CW && config.flip_type != FLIP_CCW)
                    altidx_global = 0;

                bpos_global = sf;

                std::string file_path;
                if (fileidx >= 0 && fileidx < (int)objdict.filelist.size())
                    file_path = objdict.filelist[fileidx];

                std::string chain = apply_template_to_item(gs->template_chain, soffs, playrate, file_path, loop);

                int par = (bfidx_global + item_count_global) % 2;

                if (config.alt_flip && config.flip_type != 0) {
                    int sn = 0; for (char c : chain) if (c == '[') sn++;
                    std::ostringstream fe;
                    fe << "[0." << sn << "]\neffect.name=反転\n";

                    if (config.flip_type == FLIP_HORIZONTAL) {
                        bool active = (par == 1 && altidx_global % 2 == 0) ||
                                      (par == 0 && altidx_global % 2 == 1);
                        fe << "左右反転=" << (active ? "1" : "0") << "\n";
                        fe << "上下反転=0\n";
                    } else if (config.flip_type == FLIP_VERTICAL) {
                        bool active = (par == 1 && altidx_global % 2 == 0) ||
                                      (par == 0 && altidx_global % 2 == 1);
                        fe << "左右反転=0\n";
                        fe << "上下反転=" << (active ? "1" : "0") << "\n";
                    } else if (config.flip_type == FLIP_CW) {
                        int r = item_count_global % 4;
                        fe << "左右反転=" << ((r == 1 || r == 2) ? "1" : "0") << "\n";
                        fe << "上下反転=" << ((r == 2 || r == 3) ? "1" : "0") << "\n";
                    } else if (config.flip_type == FLIP_CCW) {
                        int r = item_count_global % 4;
                        fe << "左右反転=" << ((r == 2 || r == 3) ? "1" : "0") << "\n";
                        fe << "上下反転=" << ((r == 1 || r == 2) ? "1" : "0") << "\n";
                    }

                    fe << "輝度反転=0\n色相反転=0\n透明度反転=0\n";
                    chain += fe.str();
                }

                int add_layer = 0;
                if (config.even_only && par == 1) {
                    for (size_t k = 0; k < opt_layer2.size(); k++) {
                        add_layer = (int)k;
                        if (opt_layer2[k] >= obj_fp) {
                            if (k == opt_layer2.size() - 1) { opt_layer2.push_back(bf); add_layer++; break; }
                        } else { opt_layer2[k] = bf; break; }
                    }
                    if (opt_layer2.empty()) { opt_layer2.push_back(bf); }
                } else {
                    for (size_t k = 0; k < opt_layer.size(); k++) {
                        add_layer = (int)k;
                        if (opt_layer[k] >= obj_fp) {
                            if (k == opt_layer.size() - 1) { opt_layer.push_back(bf); add_layer++; break; }
                        } else { opt_layer[k] = bf; break; }
                    }
                    if (opt_layer.empty()) { opt_layer.push_back(bf); }
                }

                int use_layer;
                if (config.even_only)
                    use_layer = base + (par == 0 ? add_layer * 2 : add_layer * 2 + 1);
                else
                    use_layer = base + add_layer;

                std::ostringstream alias;
                alias << "[0]\nlayer=" << use_layer
                      << "\nframe=" << sf << "," << ef
                      << "\noverlay=1\ngroup=1\nclipping=" << config.clipping
                      << "\ncamera=" << config.is_ex_set << "\n"
                      << chain;

                auto created_obj = edit->create_object_from_alias(alias.str().c_str(), use_layer, sf, ef - sf);
                if (created_obj) {
                    std::wstring obj_name;
                    if (!file_path.empty()) {
                        obj_name = utf8_to_wide(file_path);
                        size_t sep = obj_name.find_last_of(L'\\');
                        if (sep != std::wstring::npos) obj_name = obj_name.substr(sep + 1);
                    } else {
                        obj_name = L"Item " + std::to_wstring(item_count_global);
                    }
                    edit->set_object_name(created_obj, obj_name.c_str());
                }
                gs->created++;
                item_count_global++;
            }
            if (item_count_global > prev_count)
                item_start = track_end + 1;
            else
                item_start = track_end;
        }

        if (gs->logger)
            gs->logger->log(gs->logger, (L"AutoZWJ: 已生成 " + std::to_wstring(gs->created) + L" 个物件").c_str());
        delete gs;
    });
}

static void on_select_project(EDIT_SECTION* edit) {
    update_scene_from_edit(edit);
    show_file_picker(GetActiveWindow(), [](const std::wstring& path) {
        if (parse_project_file(path)) {
            if (g_logger)
                g_logger->log(g_logger, (L"AutoZWJ: " + get_project_summary()).c_str());
        }
    });
}

static void on_open_config(EDIT_SECTION* edit) {
    update_scene_from_edit(edit);

    int sel_num = edit->get_selected_object_num();
    if (sel_num <= 0) {
        if (g_logger) g_logger->error(g_logger, L"AutoZWJ: 请先在时间轴上选择一个物件作为模板，再右键打开配置");
        return;
    }

    auto obj = edit->get_selected_object(0);
    if (!obj) {
        if (g_logger) g_logger->error(g_logger, L"AutoZWJ: 无法获取选中物件");
        return;
    }

    LPCSTR alias_ptr = edit->get_object_alias(obj);
    if (!alias_ptr) {
        if (g_logger) g_logger->error(g_logger, L"AutoZWJ: 无法读取模板物件数据");
        return;
    }
    g_project_state.template_alias = alias_ptr;
    auto lf = edit->get_object_layer_frame(obj);
    g_project_state.template_layer = lf.layer;

    if (!g_project_state.has_data) {
        on_select_project(edit);
        return;
    }
    imgui_window_show();
}

static void on_file_drop(EDIT_SECTION* edit, LPCWSTR file) {
    update_scene_from_edit(edit);
    if (parse_project_file(file)) {
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

    host->register_layer_menu(L"选择音频工程...", on_select_project);
    host->register_object_menu(L"配置导入...", on_open_config);
    host->register_file_drop_handler(L"RPP/MIDI Input", L"*.rpp;*.mid", on_file_drop);

    g_edit_handle = host->create_edit_handle();

    HWND host_wnd = g_edit_handle->get_host_app_window();
    imgui_window_init(g_dll_hinst, host_wnd);
    imgui_window_set_generate_callback(on_generate_from_imgui);

    if (g_logger) g_logger->log(g_logger, L"AutoZWJ plugin registered");
}
