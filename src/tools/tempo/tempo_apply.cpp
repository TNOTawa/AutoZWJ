#include "tempo_apply.h"
#include "plugin.h"
#include "i18n/i18n.h"
#include "codec/codec.h"
#include "core/app_message.h"
#include "parsers/tempo_convert.h"
#include <vector>
#include <format>
#include <cmath>
#include <limits>

struct TimelineMarker {
    int frame;
    std::wstring memo;
};

struct BpmApplyParam {
    std::vector<BPM_INFO> bpm_infos;
    std::vector<TimelineMarker> markers;
};

static void bpm_edit_callback(void* p, EDIT_SECTION* edit) {
    auto* bp = static_cast<BpmApplyParam*>(p);
    edit->set_grid_bpm_list(bp->bpm_infos.data(), (int)bp->bpm_infos.size(), (int)sizeof(BPM_INFO));
    for (const auto& marker : bp->markers) {
        edit->set_mark_frame(marker.frame, marker.memo.c_str());
    }
}

static int marker_frame(double time_sec, double base_time_sec, double fps, bool use_round_up) {
    // EDIT_SECTION mark APIs use zero-based data frames, matching object creation.
    double frame_fp = (time_sec + base_time_sec) * fps;
    if (!std::isfinite(frame_fp)) return -1;

    double frame = use_round_up ? std::ceil(frame_fp) : std::round(frame_fp);
    if (frame < 0.0 || frame > (double)std::numeric_limits<int>::max()) return -1;
    return (int)frame;
}

void apply_bpm_grid() {
    if (!g_app.project.has_data) {
        app_msg_set(AppMsgSeverity::Error, tr_str(u8"请先导入工程文件"));
        return;
    }
    if (!g_host.edit_handle) {
        app_msg_set(AppMsgSeverity::Error, tr_str(u8"edit handle 不可用"));
        if (g_host.logger) g_host.logger->error(g_host.logger, utf8_to_wide(tr_str(u8"edit handle 不可用")).c_str());
        return;
    }
    if (g_app.project.objdict.tempo_map.empty()) {
        app_msg_set(AppMsgSeverity::Warn, tr_str(u8"当前工程不包含 tempo 信息"));
        if (g_host.logger) g_host.logger->warn(g_host.logger, utf8_to_wide(tr_str(u8"当前工程不包含 tempo 信息")).c_str());
        return;
    }

    auto bpm_infos = tempo_map_to_bpm_info(
        g_app.project.objdict.tempo_map,
        g_app.project.config.base_time_sec);
    if (bpm_infos.empty()) {
        app_msg_set(AppMsgSeverity::Warn, tr_str(u8"无法从 tempo 信息生成 BPM 网格"));
        return;
    }

    BpmApplyParam param;
    param.bpm_infos = std::move(bpm_infos);
    double fps = (double)g_app.project.config.fps_num / (double)g_app.project.config.fps_den;
    if (!(fps > 0.0) || !std::isfinite(fps)) fps = 60.0;
    for (const auto& marker : g_app.project.objdict.markers) {
        int frame = marker_frame(marker.time_sec, g_app.project.config.base_time_sec,
                                 fps, g_app.project.config.use_round_up);
        if (frame >= 0) {
            param.markers.push_back({frame, utf8_to_wide(marker.memo)});
        }
    }

    int count = (int)param.bpm_infos.size();
    int marker_count = (int)param.markers.size();

    g_host.edit_handle->call_edit_section_param((void*)&param, bpm_edit_callback);

    if (g_host.logger) {
        g_host.logger->log(g_host.logger, utf8_to_wide(
            tr_fmt(u8"已应用 BPM 网格 ({} 个 tempo 标记)，同时应用 {} 个 REAPER 标记", count, marker_count)).c_str());
    }
    app_msg_set(AppMsgSeverity::Success,
        tr_fmt(u8"已应用 BPM 网格 ({} 个 tempo 标记)，同时应用 {} 个 REAPER 标记", count, marker_count));
}
