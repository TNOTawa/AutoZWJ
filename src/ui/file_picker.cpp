#include "file_picker.h"
#include <windows.h>
#include <commdlg.h>
#include <functional>

void show_file_picker(HWND parent, std::function<void(const std::wstring&)> on_selected, const std::wstring& initial_dir) {
    wchar_t path_buffer[MAX_PATH * 2] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = L"REAPER/MIDI Files\0*.rpp;*.mid\0RPP Files\0*.rpp\0MIDI Files\0*.mid\0All Files\0*.*\0";
    ofn.lpstrFile = path_buffer;
    ofn.nMaxFile = MAX_PATH * 2;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    if (!initial_dir.empty()) {
        ofn.lpstrInitialDir = initial_dir.c_str();
    }
    if (GetOpenFileNameW(&ofn)) {
        if (on_selected) on_selected(path_buffer);
    }
}
