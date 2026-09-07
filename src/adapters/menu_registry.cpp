#include "adapters/menu_registry.h"
#include "codec/codec.h"
#include <windows.h>
#include <unordered_map>

static std::vector<MenuEntry> g_entries;
static std::unordered_map<std::string, bool> g_saved_states;
static std::vector<FeatureEntry> g_features;
static std::unordered_map<std::string, bool> g_saved_feature_states;
static std::wstring g_ini_path;

std::vector<MenuEntry>& menu_registry_all() {
    return g_entries;
}

void menu_registry_upsert(const std::string& id, const std::string& label_key, bool default_enabled) {
    for (const auto& e : g_entries) {
        if (e.id == id) return;
    }
    auto it = g_saved_states.find(id);
    bool enabled = (it != g_saved_states.end()) ? it->second : default_enabled;
    g_entries.push_back({id, label_key, enabled});
}

void menu_registry_set_enabled(const std::string& id, bool enabled) {
    for (auto& e : g_entries) {
        if (e.id == id) {
            e.enabled = enabled;
            menu_registry_save();
            return;
        }
    }
}

void menu_registry_load(const std::wstring& app_data_dir) {
    g_ini_path = app_data_dir;
    if (g_ini_path.empty()) return;
    if (g_ini_path.back() != L'\\') g_ini_path += L'\\';
    g_ini_path += L"AutoZWJ.menu.ini";

    g_saved_states.clear();
    g_saved_feature_states.clear();

    wchar_t buffer[2048] = {};
    DWORD n = GetPrivateProfileSectionW(L"menus", buffer, 2048, g_ini_path.c_str());
    if (n != 0) {
        for (const wchar_t* p = buffer; *p;) {
            size_t len = wcslen(p);
            if (len == 0) break;
            std::wstring kv(p, len);
            p += len + 1;

            size_t eq = kv.find(L'=');
            if (eq == std::wstring::npos) continue;

            std::string key = wide_to_utf8(kv.substr(0, eq));
            if (key.empty()) continue;
            g_saved_states[key] = (kv.substr(eq + 1) == L"1");
        }
    }

    ZeroMemory(buffer, sizeof(buffer));
    n = GetPrivateProfileSectionW(L"features", buffer, 2048, g_ini_path.c_str());
    if (n != 0) {
        for (const wchar_t* p = buffer; *p;) {
            size_t len = wcslen(p);
            if (len == 0) break;
            std::wstring kv(p, len);
            p += len + 1;

            size_t eq = kv.find(L'=');
            if (eq == std::wstring::npos) continue;

            std::string key = wide_to_utf8(kv.substr(0, eq));
            if (key.empty()) continue;
            g_saved_feature_states[key] = (kv.substr(eq + 1) == L"1");
        }
    }
}

void menu_registry_save() {
    if (g_ini_path.empty()) return;
    for (const auto& e : g_entries) {
        WritePrivateProfileStringW(L"menus", utf8_to_wide(e.id).c_str(),
            e.enabled ? L"1" : L"0", g_ini_path.c_str());
    }
}

std::vector<FeatureEntry>& feature_registry_all() {
    return g_features;
}

void feature_registry_upsert(const std::string& id, const std::string& label_key, bool default_enabled) {
    for (const auto& e : g_features) {
        if (e.id == id) return;
    }
    auto it = g_saved_feature_states.find(id);
    bool enabled = (it != g_saved_feature_states.end()) ? it->second : default_enabled;
    g_features.push_back({id, label_key, enabled});
}

bool feature_registry_is_enabled(const std::string& id, bool default_enabled) {
    for (const auto& e : g_features) {
        if (e.id == id) return e.enabled;
    }
    auto it = g_saved_feature_states.find(id);
    return it != g_saved_feature_states.end() ? it->second : default_enabled;
}

void feature_registry_save() {
    if (g_ini_path.empty()) return;
    for (const auto& e : g_features) {
        WritePrivateProfileStringW(L"features", utf8_to_wide(e.id).c_str(),
            e.enabled ? L"1" : L"0", g_ini_path.c_str());
    }
}
