#include "codec/codec.h"
#include <windows.h>

std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

std::string cp932_to_utf8(const std::string& cp932) {
    if (cp932.empty()) return "";
    int wide_len = MultiByteToWideChar(932, 0, cp932.c_str(), -1, nullptr, 0);
    if (wide_len <= 0) return cp932;
    std::wstring wide(wide_len - 1, L'\0');
    MultiByteToWideChar(932, 0, cp932.c_str(), -1, &wide[0], wide_len);
    return wide_to_utf8(wide);
}

bool is_valid_utf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i];
        if (c < 0x80) { i++; continue; }
        size_t n = 0;
        if ((c & 0xE0) == 0xC0) n = 2;
        else if ((c & 0xF0) == 0xE0) n = 3;
        else if ((c & 0xF8) == 0xF0) n = 4;
        else return false;
        if (i + n > s.size()) return false;
        for (size_t j = 1; j < n; j++) {
            if ((s[i+j] & 0xC0) != 0x80) return false;
        }
        i += n;
    }
    return true;
}

std::string maybe_cp932_to_utf8(const std::string& s) {
    if (s.empty()) return s;
    if (is_valid_utf8(s)) return s;
    return cp932_to_utf8(s);
}
