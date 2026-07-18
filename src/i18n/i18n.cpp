#include "i18n.h"
#include <unordered_map>
#include <string>
#include <cstring>
#include <cstdio>
#include <Windows.h>

#include "codec/codec.h"

static const char* kSettingsPath = nullptr;
static std::unordered_map<std::string, std::string> g_translation[3];
static Lang g_current_lang = Lang::ZhCN;

static void trim(std::string& s) {
    while (!s.empty() && (unsigned char)s.front() <= ' ') s.erase(0, 1);
    while (!s.empty() && (unsigned char)s.back() <= ' ') s.pop_back();
}

static void parse_ini_text(const char* text, int lang_idx) {
    if (!text) return;
    std::string line;
    for (const char* p = text; *p; p++) {
        if (*p == '\n' || *p == '\r') {
            if (!line.empty()) {
                trim(line);
                if (!line.empty() && line[0] != '#' && line[0] != '[') {
                    size_t eq = line.find('=');
                    if (eq != std::string::npos) {
                        std::string key = line.substr(0, eq);
                        std::string val = line.substr(eq + 1);
                        trim(key); trim(val);
                        if (!key.empty()) {
                            g_translation[lang_idx][key] = val;
                        }
                    }
                }
                line.clear();
            }
            if (*p == '\r' && *(p + 1) == '\n') p++;
        } else {
            line += *p;
        }
    }
    if (!line.empty()) {
        trim(line);
        if (!line.empty() && line[0] != '#' && line[0] != '[') {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                trim(key); trim(val);
                if (!key.empty()) {
                    g_translation[lang_idx][key] = val;
                }
            }
        }
    }
}

#include "i18n_embedded.h"

static void load_embedded() {
    parse_ini_text(kLangZhCN, 0);
    parse_ini_text(kLangEn,   1);
    parse_ini_text(kLangJa,   2);
}

static void build_settings_path() {
    static char path_buf[MAX_PATH * 2];
    wchar_t wpath[MAX_PATH];
    ExpandEnvironmentStringsW(L"%APPDATA%\\AutoZWJ", wpath, MAX_PATH);
    CreateDirectoryW(wpath, nullptr);
    int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path_buf, sizeof(path_buf), nullptr, nullptr);
    if (len > 0 && (size_t)len + 10 < sizeof(path_buf)) {
        path_buf[len - 1] = '\\';
        strcpy(path_buf + len, "settings.ini");
    }
    kSettingsPath = path_buf;
}

static Lang detect_sys_lang() {
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {};
    if (!GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH)) return Lang::ZhCN;
    if (wcsncmp(locale, L"zh", 2) == 0) return Lang::ZhCN;
    if (wcsncmp(locale, L"ja", 2) == 0) return Lang::Ja;
    if (wcsncmp(locale, L"en", 2) == 0) return Lang::En;
    return Lang::ZhCN;
}

static void load_settings() {
    build_settings_path();
    if (!kSettingsPath) return;

    wchar_t val[64] = {};
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, kSettingsPath, -1, wpath, MAX_PATH);
    GetPrivateProfileStringW(L"AutoZWJ", L"language", L"", val, 64, wpath);

    if (val[0]) {
        if (wcscmp(val, L"zh-CN") == 0)      g_current_lang = Lang::ZhCN;
        else if (wcscmp(val, L"en") == 0)    g_current_lang = Lang::En;
        else if (wcscmp(val, L"ja") == 0)    g_current_lang = Lang::Ja;
        else g_current_lang = detect_sys_lang();
    } else {
        g_current_lang = detect_sys_lang();
    }
}

static void save_settings() {
    if (!kSettingsPath) return;
    const wchar_t* val = L"zh-CN";
    if (g_current_lang == Lang::En) val = L"en";
    if (g_current_lang == Lang::Ja) val = L"ja";
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, kSettingsPath, -1, wpath, MAX_PATH);
    WritePrivateProfileStringW(L"AutoZWJ", L"language", val, wpath);
}

void i18n_init() {
    load_embedded();
    load_settings();
}

const char* tr(const char* zh_key) {
    if (!zh_key) return "";
    auto& table = g_translation[static_cast<int>(g_current_lang)];
    auto it = table.find(zh_key);
    if (it != table.end() && !it->second.empty()) return it->second.c_str();
    return zh_key;
}

std::string tr_str(const char* zh_key) {
    return std::string(tr(zh_key));
}

void set_language(Lang lang) {
    g_current_lang = lang;
    save_settings();
}

Lang get_language() {
    return g_current_lang;
}

int get_language_index() {
    return static_cast<int>(g_current_lang);
}
