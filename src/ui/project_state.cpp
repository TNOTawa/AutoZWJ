// TODO: 重构时将所有 PROJECT_FILE 键名 rppinexo.* 统一替换为 autozwj.*。
//      当前出于兼容性考虑暂不替换，新功能继续沿用 rppinexo.* 前缀。

#include "plugin.h"
#include "parsers/rpp/rpp_parser.h"
#include "parsers/midi/midi_parser.h"
#include "parsers/lrc/lrc_parser.h"
#include <algorithm>

static bool g_project_state_dirty = false;

void load_project_state_from_project_file(EDIT_SECTION* edit) {
    if (!edit) return;
    auto* pf = edit->get_project_file(g_edit_handle);
    if (!pf) return;

    g_project_state_dirty = false;
    g_project_state = ProjectState{};

    OutputConfig& cfg = g_project_state.config;
    auto val = pf->get_param_string("rppinexo.fps_num");
    if (val) cfg.fps_num = std::atoi(val);
    val = pf->get_param_string("rppinexo.fps_den");
    if (val) cfg.fps_den = std::atoi(val);
    val = pf->get_param_string("rppinexo.output_type");
    if (val) cfg.output_type = std::atoi(val);
    val = pf->get_param_string("rppinexo.alt_flip");
    if (val) cfg.alt_flip = (std::atoi(val) != 0);
    val = pf->get_param_string("rppinexo.flip_type");
    if (val) cfg.flip_type = std::atoi(val);
    val = pf->get_param_string("rppinexo.flip_counter_mode");
    if (val) cfg.flip_counter_mode = std::atoi(val);
    val = pf->get_param_string("rppinexo.adjustment");
    if (val) cfg.adjustment = (std::atoi(val) != 0);
    val = pf->get_param_string("rppinexo.beatless_sync");
    if (val) cfg.beatless_sync = (std::atoi(val) != 0);
    val = pf->get_param_string("rppinexo.clipping");
    if (val) cfg.clipping = std::atoi(val);
    val = pf->get_param_string("rppinexo.is_ex_set");
    if (val) cfg.is_ex_set = std::atoi(val);
    val = pf->get_param_string("rppinexo.exedit_lang");
    if (val) cfg.exedit_lang = val;
    val = pf->get_param_string("rppinexo.mapping_strategy");
    if (val) cfg.mapping_strategy = std::atoi(val);
    val = pf->get_param_string("rppinexo.mapping_sequential_order");
    if (val) cfg.mapping_sequential_order = std::atoi(val);
    val = pf->get_param_string("rppinexo.mapping_no_consecutive");
    if (val) cfg.mapping_no_consecutive = (std::atoi(val) != 0);
    val = pf->get_param_string("rppinexo.last_directory");
    if (val) g_project_state.last_directory = utf8_to_wide(val);

    val = pf->get_param_string("rppinexo.history_count");
    int count = val ? std::atoi(val) : 0;
    if (count < 0) count = 0;
    if (count > 10) count = 10;
    g_project_state.file_history.clear();
    for (int i = 0; i < count; i++) {
        std::string key = "rppinexo.history_" + std::to_string(i);
        val = pf->get_param_string(key.c_str());
        if (val && val[0]) {
            FileHistoryEntry entry;
            entry.path = utf8_to_wide(val);
            std::string offset_key = "rppinexo.history_offset_" + std::to_string(i);
            auto oval = pf->get_param_string(offset_key.c_str());
            if (oval && oval[0]) {
                entry.base_time_sec = std::atof(oval);
            }
            g_project_state.file_history.push_back(entry);
        }
    }

    val = pf->get_param_string("rppinexo.file_path");
    if (val) g_project_state.file_path = utf8_to_wide(val);

    for (auto& entry : g_project_state.file_history) {
        if (entry.path == g_project_state.file_path) {
            g_project_state.config.base_time_sec = entry.base_time_sec;
            break;
        }
    }

}

void save_project_file_path_and_history(EDIT_SECTION* edit) {
    if (!edit) return;
    auto* pf = edit->get_project_file(g_edit_handle);
    if (!pf) return;
    pf->set_param_string("rppinexo.file_path", wide_to_utf8(g_project_state.file_path).c_str());
    int count = (int)std::min(g_project_state.file_history.size(), size_t(10));
    pf->set_param_string("rppinexo.history_count", std::to_string(count).c_str());
    for (int i = 0; i < 10; i++) {
        std::string key = "rppinexo.history_" + std::to_string(i);
        std::string offset_key = "rppinexo.history_offset_" + std::to_string(i);
        if (i < count) {
            pf->set_param_string(key.c_str(), wide_to_utf8(g_project_state.file_history[i].path).c_str());
            pf->set_param_string(offset_key.c_str(), std::to_string(g_project_state.file_history[i].base_time_sec).c_str());
        } else {
            pf->set_param_string(key.c_str(), "");
            pf->set_param_string(offset_key.c_str(), "");
        }
    }
}

void flush_project_file_state(EDIT_SECTION* edit) {
    if (!g_project_state_dirty || !edit) return;
    save_project_file_path_and_history(edit);
    g_project_state_dirty = false;
}

void add_file_to_history(const std::wstring& path) {
    if (path.empty()) return;
    auto it = std::find_if(g_project_state.file_history.begin(), g_project_state.file_history.end(),
        [&](const FileHistoryEntry& e) { return e.path == path; });
    if (it != g_project_state.file_history.end()) {
        g_project_state.file_history.erase(it);
    }
    g_project_state.file_history.insert(g_project_state.file_history.begin(), FileHistoryEntry{path, g_project_state.config.base_time_sec});
    if (g_project_state.file_history.size() > 10) {
        g_project_state.file_history.resize(10);
    }
    g_project_state_dirty = true;
}

void remove_file_from_history(int index) {
    if (index < 0 || index >= (int)g_project_state.file_history.size()) return;
    g_project_state.file_history.erase(g_project_state.file_history.begin() + index);
    g_project_state_dirty = true;
}

void update_current_project_offset(double offset) {
    g_project_state.config.base_time_sec = offset;
    auto& file_path = g_project_state.file_path;
    if (file_path.empty()) return;
    auto it = std::find_if(g_project_state.file_history.begin(), g_project_state.file_history.end(),
        [&](const FileHistoryEntry& e) { return e.path == file_path; });
    if (it != g_project_state.file_history.end()) {
        it->base_time_sec = offset;
    } else {
        g_project_state.file_history.insert(g_project_state.file_history.begin(), FileHistoryEntry{file_path, offset});
        if (g_project_state.file_history.size() > 10) {
            g_project_state.file_history.resize(10);
        }
    }
    g_project_state_dirty = true;
}

void save_config_to_project_file(EDIT_HANDLE* edit, const OutputConfig& cfg) {
    edit->call_edit_section_param((void*)&cfg, [](void* param, EDIT_SECTION* edit) {
        auto* cfg = static_cast<const OutputConfig*>(param);
        auto* pf = edit->get_project_file(g_edit_handle);
        if (!pf) return;
        pf->set_param_string("rppinexo.fps_num", std::to_string(cfg->fps_num).c_str());
        pf->set_param_string("rppinexo.fps_den", std::to_string(cfg->fps_den).c_str());
        pf->set_param_string("rppinexo.output_type", std::to_string(cfg->output_type).c_str());
        pf->set_param_string("rppinexo.alt_flip", cfg->alt_flip ? "1" : "0");
        pf->set_param_string("rppinexo.flip_type", std::to_string(cfg->flip_type).c_str());
        pf->set_param_string("rppinexo.flip_counter_mode", std::to_string(cfg->flip_counter_mode).c_str());
        pf->set_param_string("rppinexo.adjustment", cfg->adjustment ? "1" : "0");
        pf->set_param_string("rppinexo.beatless_sync", cfg->beatless_sync ? "1" : "0");
        pf->set_param_string("rppinexo.clipping", std::to_string(cfg->clipping).c_str());
        pf->set_param_string("rppinexo.is_ex_set", std::to_string(cfg->is_ex_set).c_str());
        pf->set_param_string("rppinexo.exedit_lang", cfg->exedit_lang.c_str());
        pf->set_param_string("rppinexo.mapping_strategy", std::to_string(cfg->mapping_strategy).c_str());
        pf->set_param_string("rppinexo.mapping_sequential_order", std::to_string(cfg->mapping_sequential_order).c_str());
        pf->set_param_string("rppinexo.mapping_no_consecutive", cfg->mapping_no_consecutive ? "1" : "0");
        pf->set_param_string("rppinexo.last_directory", wide_to_utf8(g_project_state.last_directory).c_str());
    });
}

bool parse_project_file(const std::wstring& file_path) {
    std::string path_utf8 = wide_to_utf8(file_path);
    std::string lower = path_utf8;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    g_project_state.file_path = file_path;
    size_t sep = file_path.find_last_of(L'\\');
    g_project_state.file_name = (sep != std::wstring::npos) ? file_path.substr(sep + 1) : file_path;

    bool existed = false;
    for (auto& entry : g_project_state.file_history) {
        if (entry.path == file_path) {
            existed = true;
            break;
        }
    }

    g_project_state.tracks.clear();
    g_project_state.objdict = ObjDict{};
    std::vector<std::string> file_paths;
    EndWarnings warnings;

    bool ok = false;
    if (lower.find(".rpp") != std::string::npos) {
        ok = parse_rpp_auto(path_utf8, g_project_state.objdict, g_project_state.tracks, file_paths, warnings);
    } else if (lower.find(".mid") != std::string::npos) {
        ok = parse_midi(path_utf8, g_project_state.objdict, g_project_state.tracks, file_paths);
    } else if (lower.find(".lrc") != std::string::npos) {
        ok = parse_lrc(path_utf8, g_project_state.objdict, g_project_state.tracks, file_paths);
    }

    if (ok) {
        g_project_state.objdict.filelist = file_paths;
        g_project_state.has_data = true;
        deselect_all_tracks();
        if (existed) {
            for (auto& entry : g_project_state.file_history) {
                if (entry.path == file_path) {
                    g_project_state.config.base_time_sec = entry.base_time_sec;
                    break;
                }
            }
        } else {
            g_project_state.config.base_time_sec = 0.0;
        }
        add_file_to_history(file_path);
    }
    return ok;
}

std::wstring get_project_summary() {
    if (!g_project_state.has_data) return L"";
    int item_count = 0;
    for (size_t i = 1; i < g_project_state.objdict.pos.size(); i++) {
        if (g_project_state.objdict.pos[i] != -1.0) item_count++;
    }
    std::wstring summary = g_project_state.file_name + L" | ";
    summary += std::to_wstring((int)g_project_state.tracks.size()) + L" Track, ";
    summary += std::to_wstring(item_count) + L" Item";
    if (g_project_state.objdict.bpm > 0) {
        summary += L", " + std::to_wstring((int)(g_project_state.objdict.bpm + 0.5)) + L" BPM";
    }
    return summary;
}

static void apply_to_all_tracks(std::vector<TrackNode>& nodes, bool selected) {
    for (auto& n : nodes) {
        n.selected = selected;
        apply_to_all_tracks(n.children, selected);
    }
}

void select_all_tracks() {
    apply_to_all_tracks(g_project_state.tracks, true);
}

void deselect_all_tracks() {
    apply_to_all_tracks(g_project_state.tracks, false);
}

static void invert_tracks(std::vector<TrackNode>& nodes) {
    for (auto& n : nodes) {
        n.selected = !n.selected;
        invert_tracks(n.children);
    }
}

void invert_track_selection() {
    invert_tracks(g_project_state.tracks);
}
