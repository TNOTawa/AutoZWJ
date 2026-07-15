#pragma once

// 将当前工程的 tempo_map 写入 AviUtl2 的 BPM 网格。
// 读取 g_project_state.objdict.tempo_map 和 g_project_state.config.base_time_sec。
// 需在 g_edit_handle 可用时调用（ImGui 渲染线程或 EDIT_SECTION 回调内）。
void apply_bpm_grid();
