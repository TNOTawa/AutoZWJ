#pragma once
#include "imgui.h"
#include "plugin.h"
#include <vector>
#include <string>

enum class AppPage {
    Import,
    Config,
};

extern AppPage g_current_page;

void render_nav_bar();
void render_import_page();
void render_config_page();

// 轨道树渲染，read_only=true 时不可交互（仅预览）
void render_track_tree(bool read_only = false);
void render_config_panel();
void render_action_bar();

// 目录持久化
std::wstring load_last_directory();
void save_last_directory(const std::wstring& dir);
