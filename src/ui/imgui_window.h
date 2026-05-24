#pragma once
#include <windows.h>

bool imgui_window_init(HINSTANCE hinst, HWND host_window);
void imgui_window_show();
void imgui_window_hide();
bool imgui_window_is_visible();
void imgui_window_shutdown();
void imgui_window_set_generate_callback(void (*cb)());
void imgui_window_trigger_generate();