#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

#include "codec/codec.h"
#include "core/project_model.h"
#include "core/generation_model.h"
#include "core/effect_model.h"
#include "core/output_config.h"
#include "host/host_context.h"
#include "core/app_state.h"
#include "application/project_controller.h"
#include "adapters/project_file_store.h"
#include "core/preferences.h"

// TODO: 重构时将所有 PROJECT_FILE 键名 rppinexo.* 统一替换为 autozwj.*。
//      当前出于兼容性考虑暂不替换，新功能继续沿用 rppinexo.* 前缀。

extern AppState g_app;
extern HostContext g_host;

void sync_scene_info();

std::string host_translate_effect_name(const std::string& ja_name);
bool host_get_default_font(std::wstring& name);
bool host_font_supports_cjk(const std::wstring& name);
