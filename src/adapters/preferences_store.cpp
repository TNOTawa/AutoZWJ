#include "core/preferences.h"
#include "adapters/menu_registry.h"
#include <windows.h>
#include <algorithm>

static Preferences g_preferences = default_preferences();
static std::wstring g_ini_path;

Preferences default_preferences() { return Preferences{}; }
Preferences& preferences() { return g_preferences; }

static int read_int(const wchar_t* key, int fallback) {
    wchar_t buf[64] = {};
    GetPrivateProfileStringW(L"preferences", key, L"", buf, 64, g_ini_path.c_str());
    return buf[0] ? _wtoi(buf) : fallback;
}

void preferences_load(const std::wstring& app_data_dir) {
    g_preferences = default_preferences();
    g_ini_path = app_data_dir;
    if (g_ini_path.empty()) return;
    if (g_ini_path.back() != L'\\') g_ini_path += L'\\';
    g_ini_path += L"AutoZWJ.preferences.ini";

    wchar_t xmidi[64] = {};
    GetPrivateProfileStringW(L"preferences", L"parse_rpp_xmidi_notes", L"", xmidi, 64, g_ini_path.c_str());
    if (xmidi[0]) {
        g_preferences.parse_rpp_xmidi_notes = _wtoi(xmidi) != 0;
    } else {
        bool old_value = true;
        if (feature_registry_saved_state("parser.rpp_xmidi_notes", old_value)) {
            g_preferences.parse_rpp_xmidi_notes = old_value;
            preferences_save();
        }
    }
    g_preferences.window_width = std::clamp(read_int(L"window_width", Preferences::kDefaultWindowWidth), 640, 3840);
    g_preferences.window_height = std::clamp(read_int(L"window_height", Preferences::kDefaultWindowHeight), 420, 2160);
    g_preferences.effect_editor_default = read_int(L"effect_editor_default", 0) != 0;

    wchar_t font[256] = {};
    GetPrivateProfileStringW(L"preferences", L"font_name", L"", font, 256, g_ini_path.c_str());
    g_preferences.font_name = font;
    wchar_t size[64] = {};
    GetPrivateProfileStringW(L"preferences", L"font_size", L"", size, 64, g_ini_path.c_str());
    if (size[0]) g_preferences.font_size = std::clamp(static_cast<float>(_wtof(size)), 10.0f, 48.0f);
    g_preferences.font_auto_setup_done = read_int(L"font_auto_setup_done", 0) != 0;
}

void preferences_save() {
    if (g_ini_path.empty()) return;
    auto put = [](const wchar_t* key, const std::wstring& value) {
        WritePrivateProfileStringW(L"preferences", key, value.c_str(), g_ini_path.c_str());
    };
    put(L"parse_rpp_xmidi_notes", g_preferences.parse_rpp_xmidi_notes ? L"1" : L"0");
    put(L"window_width", std::to_wstring(g_preferences.window_width));
    put(L"window_height", std::to_wstring(g_preferences.window_height));
    put(L"font_name", g_preferences.font_name);
    put(L"font_size", std::to_wstring(g_preferences.font_size));
    put(L"font_auto_setup_done", g_preferences.font_auto_setup_done ? L"1" : L"0");
    put(L"effect_editor_default", g_preferences.effect_editor_default ? L"1" : L"0");
}

void preferences_reset_all() { g_preferences = default_preferences(); preferences_save(); }
void preferences_reset_xmidi() { g_preferences.parse_rpp_xmidi_notes = default_preferences().parse_rpp_xmidi_notes; preferences_save(); }
void preferences_reset_window_size() { g_preferences.window_width = Preferences::kDefaultWindowWidth; g_preferences.window_height = Preferences::kDefaultWindowHeight; preferences_save(); }
void preferences_reset_font() { g_preferences.font_name.clear(); preferences_save(); }
void preferences_reset_font_size() { g_preferences.font_size = Preferences::kDefaultFontSize; preferences_save(); }
void preferences_reset_effect_editor() { g_preferences.effect_editor_default = default_preferences().effect_editor_default; preferences_save(); }
