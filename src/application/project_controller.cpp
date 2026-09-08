#include "project_controller.h"
#include "import_service.h"
#include "core/app_message.h"
#include "i18n/i18n.h"
#include "codec/codec.h"
#include <algorithm>

bool g_project_state_dirty = false;

bool select_project(AppState& app, const std::wstring& file_path) {
    auto parsed = parse_source(file_path);
    if (parsed.tracks.empty() && parsed.objdict.pos.empty()) {
        app_msg_set(AppMsgSeverity::Error, tr_str(u8"无法解析该文件，仅支持 .rpp / .mid / .ust / .ustx / .lrc"));
        return false;
    }

    app.project.file_path = file_path;
    size_t sep = file_path.find_last_of(L'\\');
    app.project.file_name = (sep != std::wstring::npos) ? file_path.substr(sep + 1) : file_path;

    app_msg_set(AppMsgSeverity::Success,
        tr_fmt(u8"已加载工程：{}", wide_to_utf8(app.project.file_name)));

    bool existed = false;
    for (auto& entry : app.project.file_history) {
        if (entry.path == file_path) {
            existed = true;
            break;
        }
    }

    app.project.tracks = std::move(parsed.tracks);
    app.project.objdict = std::move(parsed.objdict);
    app.project.objdict.filelist = std::move(parsed.file_paths);
    app.project.has_data = true;

    deselect_all_tracks(app);

    if (existed) {
        for (auto& entry : app.project.file_history) {
            if (entry.path == file_path) {
                app.project.config.base_time_sec = entry.base_time_sec;
                break;
            }
        }
    } else {
        app.project.config.base_time_sec = 0.0;
    }
    add_file_to_history(app, file_path);
    return true;
}

void add_file_to_history(AppState& app, const std::wstring& path) {
    if (path.empty()) return;
    auto it = std::find_if(app.project.file_history.begin(), app.project.file_history.end(),
        [&](const FileHistoryEntry& e) { return e.path == path; });
    if (it != app.project.file_history.end()) {
        app.project.file_history.erase(it);
    }
    app.project.file_history.insert(app.project.file_history.begin(), FileHistoryEntry{path, app.project.config.base_time_sec});
    if (app.project.file_history.size() > 10) {
        app.project.file_history.resize(10);
    }
    g_project_state_dirty = true;
}

void remove_file_from_history(AppState& app, int index) {
    if (index < 0 || index >= (int)app.project.file_history.size()) return;
    app.project.file_history.erase(app.project.file_history.begin() + index);
    g_project_state_dirty = true;
}

void update_current_project_offset(AppState& app, double offset) {
    app.project.config.base_time_sec = offset;
    auto& fp = app.project.file_path;
    if (fp.empty()) return;
    auto it = std::find_if(app.project.file_history.begin(), app.project.file_history.end(),
        [&](const FileHistoryEntry& e) { return e.path == fp; });
    if (it != app.project.file_history.end()) {
        it->base_time_sec = offset;
    } else {
        app.project.file_history.insert(app.project.file_history.begin(), FileHistoryEntry{fp, offset});
        if (app.project.file_history.size() > 10) {
            app.project.file_history.resize(10);
        }
    }
    g_project_state_dirty = true;
}

static void apply_to_all_tracks(std::vector<TrackNode>& nodes, bool selected) {
    for (auto& n : nodes) {
        n.selected = selected;
        apply_to_all_tracks(n.children, selected);
    }
}

void select_all_tracks(AppState& app) {
    apply_to_all_tracks(app.project.tracks, true);
}

void deselect_all_tracks(AppState& app) {
    apply_to_all_tracks(app.project.tracks, false);
}

static void invert_tracks(std::vector<TrackNode>& nodes) {
    for (auto& n : nodes) {
        n.selected = !n.selected;
        invert_tracks(n.children);
    }
}

void invert_track_selection(AppState& app) {
    invert_tracks(app.project.tracks);
}

std::wstring get_project_summary(const AppState& app) {
    if (!app.project.has_data) return L"";
    int item_count = 0;
    for (size_t i = 1; i < app.project.objdict.pos.size(); i++) {
        if (app.project.objdict.pos[i] != -1.0) item_count++;
    }
    std::wstring summary = app.project.file_name + L" | ";
    summary += std::to_wstring((int)app.project.tracks.size()) + L" Track, ";
    summary += std::to_wstring(item_count) + L" Item";
    if (app.project.objdict.bpm > 0) {
        summary += L", " + std::to_wstring((int)(app.project.objdict.bpm + 0.5)) + L" BPM";
    }
    return summary;
}
