#pragma once
#include <string>
#include <format>

enum class Lang : int { ZhCN = 0, En = 1, Ja = 2 };

using HostLangDetector = Lang (*)();

void i18n_init();
const char* tr(const char* zh_key);
std::string tr_str(const char* zh_key);
void set_language(Lang lang);
Lang get_language();
int get_language_index();
void i18n_set_host_lang_detector(HostLangDetector detector);

template<typename... Args>
std::string tr_fmt(const char* zh_key, Args&&... args) {
    return std::vformat(tr_str(zh_key), std::make_format_args(args...));
}
