#pragma once

#include <cstdlib>
#include <cctype>
#include <string>

inline std::string chain_trim_motion_token(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

inline bool chain_is_number_token(const std::string& value) {
    std::string token = chain_trim_motion_token(value);
    if (token.empty()) return false;
    char* end = nullptr;
    std::strtod(token.c_str(), &end);
    return end != token.c_str() && *end == '\0';
}

// AviUtl2 motion values are start,end,method,retain|curve. Requiring the
// fourth field avoids treating ordinary RGB/RGBA values as motion parameters.
inline bool chain_parse_motion_value(const std::string& value,
                                     std::string* out_start,
                                     std::string* out_end,
                                     std::string* out_rest,
                                     bool require_motion_tail = false) {
    size_t c1 = value.find(',');
    if (c1 == std::string::npos) return false;
    size_t c2 = value.find(',', c1 + 1);
    if (c2 == std::string::npos) return false;
    std::string start = chain_trim_motion_token(value.substr(0, c1));
    std::string end = chain_trim_motion_token(value.substr(c1 + 1, c2 - c1 - 1));
    if (!chain_is_number_token(start) || !chain_is_number_token(end)) return false;

    if (require_motion_tail) {
        size_t c3 = value.find(',', c2 + 1);
        if (c3 == std::string::npos) return false;
        std::string tail = chain_trim_motion_token(value.substr(c3 + 1));
        if (tail.empty()) return false;
        if (chain_is_number_token(tail)) {
            double retain = std::strtod(tail.c_str(), nullptr);
            if (retain < 0.0 || retain > 1.0) return false;
        } else if (tail.find('|') == std::string::npos) {
            return false;
        }
    }

    if (out_start) *out_start = start;
    if (out_end) *out_end = end;
    if (out_rest) *out_rest = value.substr(c2);
    return true;
}
