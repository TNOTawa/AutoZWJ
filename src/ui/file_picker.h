#pragma once
#include "plugin.h"
#include <functional>

void show_file_picker(HWND parent, std::function<void(const std::wstring&)> on_selected, const std::wstring& initial_dir = L"");
