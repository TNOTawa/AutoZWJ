#include "tempo_apply.h"
#include "plugin.h"
#include "parsers/tempo_convert.h"
#include <vector>

struct BpmApplyParam {
    std::vector<BPM_INFO> bpm_infos;
};

void apply_bpm_grid() {
    if (!g_project_state.has_data) return;
    if (!g_edit_handle) {
        if (g_logger) g_logger->error(g_logger, L"AutoZWJ: edit handle 不可用");
        return;
    }
    if (g_project_state.objdict.tempo_map.empty()) return;

    auto bpm_infos = tempo_map_to_bpm_info(
        g_project_state.objdict.tempo_map,
        g_project_state.config.base_time_sec);
    if (bpm_infos.empty()) return;

    BpmApplyParam param;
    param.bpm_infos = std::move(bpm_infos);
    int count = (int)param.bpm_infos.size();

    g_edit_handle->call_edit_section_param((void*)&param, [](void* p, EDIT_SECTION* edit) {
        auto* bp = static_cast<BpmApplyParam*>(p);
        edit->set_grid_bpm_list(bp->bpm_infos.data(), (int)bp->bpm_infos.size(), (int)sizeof(BPM_INFO));
    });

    if (g_logger) {
        g_logger->log(g_logger, (L"AutoZWJ: 已应用 BPM 网格 (" +
            std::to_wstring(count) + L" 个 tempo 标记)").c_str());
    }
}
