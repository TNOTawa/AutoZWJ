#pragma once
#include <string>

std::wstring utf8_to_wide(const std::string& utf8);
std::string wide_to_utf8(const std::wstring& wide);
std::string cp932_to_utf8(const std::string& cp932);
std::string maybe_cp932_to_utf8(const std::string& s);
bool is_valid_utf8(const std::string& s);
