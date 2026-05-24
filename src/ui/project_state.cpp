#include "plugin.h"
#include "rpp/rpp_parser.h"
#include "midi/midi_parser.h"
#include <algorithm>

static void load_config_from_project_file(EDIT_HANDLE* edit) {
    OutputConfig& cfg = g_project_state.config;
    edit->call_read_section_param(&cfg, [](void* param, EDIT_SECTION* edit) {
        auto* cfg = static_cast<OutputConfig*>(param);
        auto* pf = edit->get_project_file(nullptr);
        if (!pf) return;
        auto val = pf->get_param_string("rppinexo.fps_num");
        if (val) cfg->fps_num = std::atoi(val);
        val = pf->get_param_string("rppinexo.fps_den");
        if (val) cfg->fps_den = std::atoi(val);
        val = pf->get_param_string("rppinexo.output_type");
        if (val) cfg->output_type = std::atoi(val);
        val = pf->get_param_string("rppinexo.alt_flip");
        if (val) cfg->alt_flip = (std::atoi(val) != 0);
        val = pf->get_param_string("rppinexo.flip_type");
        if (val) cfg->flip_type = std::atoi(val);
        val = pf->get_param_string("rppinexo.even_only");
        if (val) cfg->even_only = (std::atoi(val) != 0);
        val = pf->get_param_string("rppinexo.adjustment");
        if (val) cfg->adjustment = (std::atoi(val) != 0);
        val = pf->get_param_string("rppinexo.beatless_sync");
        if (val) cfg->beatless_sync = (std::atoi(val) != 0);
        val = pf->get_param_string("rppinexo.clipping");
        if (val) cfg->clipping = std::atoi(val);
        val = pf->get_param_string("rppinexo.is_ex_set");
        if (val) cfg->is_ex_set = std::atoi(val);
        val = pf->get_param_string("rppinexo.exedit_lang");
        if (val) cfg->exedit_lang = val;
    });
}

void save_config_to_project_file(EDIT_HANDLE* edit, const OutputConfig& cfg) {
    edit->call_edit_section_param((void*)&cfg, [](void* param, EDIT_SECTION* edit) {
        auto* cfg = static_cast<const OutputConfig*>(param);
        auto* pf = edit->get_project_file(nullptr);
        if (!pf) return;
        pf->set_param_string("rppinexo.fps_num", std::to_string(cfg->fps_num).c_str());
        pf->set_param_string("rppinexo.fps_den", std::to_string(cfg->fps_den).c_str());
        pf->set_param_string("rppinexo.output_type", std::to_string(cfg->output_type).c_str());
        pf->set_param_string("rppinexo.alt_flip", cfg->alt_flip ? "1" : "0");
        pf->set_param_string("rppinexo.flip_type", std::to_string(cfg->flip_type).c_str());
        pf->set_param_string("rppinexo.even_only", cfg->even_only ? "1" : "0");
        pf->set_param_string("rppinexo.adjustment", cfg->adjustment ? "1" : "0");
        pf->set_param_string("rppinexo.beatless_sync", cfg->beatless_sync ? "1" : "0");
        pf->set_param_string("rppinexo.clipping", std::to_string(cfg->clipping).c_str());
        pf->set_param_string("rppinexo.is_ex_set", std::to_string(cfg->is_ex_set).c_str());
        pf->set_param_string("rppinexo.exedit_lang", cfg->exedit_lang.c_str());
    });
}

bool parse_project_file(const std::wstring& file_path) {
    std::string path_utf8 = wide_to_utf8(file_path);
    std::string lower = path_utf8;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    g_project_state.file_path = file_path;
    size_t sep = file_path.find_last_of(L'\\');
    g_project_state.file_name = (sep != std::wstring::npos) ? file_path.substr(sep + 1) : file_path;

    g_project_state.tracks.clear();
    g_project_state.objdict = ObjDict{};
    std::vector<std::string> file_paths;
    EndWarnings warnings;

    bool ok = false;
    if (lower.find(".rpp") != std::string::npos) {
        ok = parse_rpp_auto(path_utf8, g_project_state.objdict, g_project_state.tracks, file_paths, warnings);
    } else if (lower.find(".mid") != std::string::npos) {
        ok = parse_midi(path_utf8, g_project_state.objdict, g_project_state.tracks, file_paths);
    }

    if (ok) {
        g_project_state.objdict.filelist = file_paths;
        g_project_state.has_data = true;
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
