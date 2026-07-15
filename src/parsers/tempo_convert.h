#pragma once
#include "plugin.h"
#include <vector>
#include <algorithm>
#include <cmath>

// tempo_map (TempoPoint[]) -> BPM_INFO[]，供 set_grid_bpm_list 使用。
// base_time_sec: 基准时间偏移，叠加到每个 BPM_INFO.start 上。
// 算法在秒域统一：拍号变化重置 measure_start，tempo 变化按当前小节时长对齐。
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

    double measure_start = 0.0;
    double prev_bpm = sorted[0].bpm;
    int prev_beat = sorted[0].beat;

    for (size_t i = 0; i < sorted.size(); i++) {
        const auto& p = sorted[i];

        if (i > 0) {
            if (p.beat != prev_beat) {
                measure_start = p.time_sec;
            } else if (prev_bpm > 0.0) {
                double measure_dur = prev_beat * 60.0 / prev_bpm;
                if (measure_dur > 0.0) {
                    double elapsed = p.time_sec - measure_start;
                    measure_start += std::floor(elapsed / measure_dur) * measure_dur;
                }
            }
        }

        BPM_INFO info = {};
        info.tempo = (float)p.bpm;
        info.beat = p.beat;
        info.start = p.time_sec + base_time_sec;
        info.offset = (float)-(p.time_sec - measure_start);

        // 同时间点合并：替换最后一个
        if (!result.empty() && result.back().start == info.start) {
            result.back() = info;
        } else {
            result.push_back(info);
        }

        prev_bpm = p.bpm;
        prev_beat = p.beat;
    }

    return result;
}
