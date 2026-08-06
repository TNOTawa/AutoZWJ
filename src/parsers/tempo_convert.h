#pragma once
#include "plugin.h"
#include <vector>
#include <algorithm>

// tempo_map (TempoPoint[]) -> BPM_INFO[]，供 set_grid_bpm_list 使用。
// base_time_sec: 基准时间偏移，叠加到每个 BPM_INFO.start 上。
// AviUtl2 从每个 tempo 点的 start 起按 beat 生成拍线，offset 恒为 0；
// 同时间多事件：拍号优先于 tempo（与参考实现一致）。
inline std::vector<BPM_INFO> tempo_map_to_bpm_info(const std::vector<TempoPoint>& tempo_map,
                                                    double base_time_sec) {
    std::vector<BPM_INFO> result;
    if (tempo_map.empty()) return result;

    auto sorted = tempo_map;
    // 拍号变化优先于同时间的 tempo 变化
    std::stable_sort(sorted.begin(), sorted.end(), [](const TempoPoint& a, const TempoPoint& b) {
        return a.time_sec < b.time_sec;
    });

    for (size_t i = 0; i < sorted.size(); i++) {
        const auto& p = sorted[i];

        BPM_INFO info = {};
        info.tempo = (float)p.bpm;
        info.beat = p.beat;
        info.start = p.time_sec + base_time_sec;
        info.offset = 0.0f;

        // 同时间点合并：替换最后一个
        if (!result.empty() && result.back().start == info.start) {
            result.back() = info;
        } else {
            result.push_back(info);
        }
    }

    return result;
}
