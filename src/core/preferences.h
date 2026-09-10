#pragma once
#include <string>

struct Preferences {
    static constexpr int kDefaultWindowWidth = 1127;
    static constexpr int kDefaultWindowHeight = 650;
    static constexpr float kDefaultFontSize = 18.0f;

    bool parse_rpp_xmidi_notes = true;
    int window_width = kDefaultWindowWidth;
    int window_height = kDefaultWindowHeight;
    std::wstring font_name;
    float font_size = kDefaultFontSize;
    // 首次启动的界面字体自动分配是否已评估（中文宿主下自动分配简体中文字体）
    bool font_auto_setup_done = false;
    bool effect_editor_default = false;
};

Preferences default_preferences();
Preferences& preferences();
void preferences_load(const std::wstring& app_data_dir);
void preferences_save();
void preferences_reset_all();
void preferences_reset_xmidi();
void preferences_reset_window_size();
void preferences_reset_font();
void preferences_reset_font_size();
void preferences_reset_effect_editor();
