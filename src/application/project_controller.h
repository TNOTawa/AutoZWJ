#pragma once
#include <string>
#include "core/app_state.h"

extern bool g_project_state_dirty;

bool select_project(AppState& app, const std::wstring& file_path);

void add_file_to_history(AppState& app, const std::wstring& path);
void remove_file_from_history(AppState& app, int index);
void update_current_project_offset(AppState& app, double offset);
void deselect_all_tracks(AppState& app);
void select_all_tracks(AppState& app);
void invert_track_selection(AppState& app);
std::wstring get_project_summary(const AppState& app);