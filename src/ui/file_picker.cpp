#include "file_picker.h"
#include "i18n/i18n.h"
#include "codec/codec.h"
#include <windows.h>
#include <commdlg.h>
#include <functional>
#include <string>

void show_file_picker(HWND parent, std::function<void(const std::wstring&)> on_selected, const std::wstring& initial_dir) {
    wchar_t path_buffer[MAX_PATH * 2] = {};

    std::wstring filter;
    filter += utf8_to_wide(tr_str(u8"REAPER/MIDI Files"));
    filter += L'\0';
    filter += L"*.rpp;*.mid";
    filter += L'\0';
    filter += L"RPP Files";
    filter += L'\0';
    filter += L"*.rpp";
    filter += L'\0';
    filter += L"MIDI Files";
    filter += L'\0';
    filter += L"*.mid";
    filter += L'\0';
    filter += L"All Files";
    filter += L'\0';
    filter += L"*.*";
    filter += L'\0';

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter.c_str();
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
