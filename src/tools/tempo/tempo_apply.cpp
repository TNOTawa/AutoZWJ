#include "tempo_apply.h"
#include "plugin.h"
#include "i18n/i18n.h"
#include "codec/codec.h"
#include "core/app_message.h"
#include "parsers/tempo_convert.h"
#include <vector>
#include <format>

struct BpmApplyParam {
    std::vector<BPM_INFO> bpm_infos;
};

static void bpm_edit_callback(void* p, EDIT_SECTION* edit) {
    auto* bp = static_cast<BpmApplyParam*>(p);
    edit->set_grid_bpm_list(bp->bpm_infos.data(), (int)bp->bpm_infos.size(), (int)sizeof(BPM_INFO));
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
    int count = (int)param.bpm_infos.size();

    g_host.edit_handle->call_edit_section_param((void*)&param, bpm_edit_callback);

    if (g_host.logger) {
        g_host.logger->log(g_host.logger, utf8_to_wide(tr_fmt(u8"已应用 BPM 网格 ({} 个 tempo 标记)", count)).c_str());
    }
    app_msg_set(AppMsgSeverity::Success, tr_fmt(u8"已应用 BPM 网格 ({} 个 tempo 标记)", count));
}
