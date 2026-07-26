#pragma once
#include "host/host_context.h"
#include "core/app_state.h"
#include "core/output_config.h"

void save_config_to_project_file(const OutputConfig& cfg, const AppState& app, const HostContext& host);
void load_project_state_from_project_file(EDIT_SECTION* edit, AppState& app, const HostContext& host);
void save_project_file_path_and_history(EDIT_SECTION* edit, const AppState& app, const HostContext& host);
void flush_project_file_state(EDIT_SECTION* edit, const AppState& app, const HostContext& host);