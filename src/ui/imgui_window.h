#pragma once
#include <windows.h>

bool imgui_window_init(HINSTANCE hinst, HWND host_window);
void imgui_window_show();
void imgui_window_hide();
bool imgui_window_is_visible();
void imgui_window_shutdown();
void imgui_window_set_generate_callback(bool (*cb)());
bool imgui_window_trigger_generate();
void imgui_window_show_import_page();
// 重建字体集（字体名/主题变化时用，字号变化由逐帧同步处理）
void imgui_window_reload_fonts();
// 按偏好尺寸设置窗口大小（仅窗口尺寸相关操作调用，不含字体变更）
void imgui_window_apply_window_size();
