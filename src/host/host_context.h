#pragma once

// ===================================================================
// 宿主 SDK 入口：经本头间接包含 windows.h / plugin2.h / logger2.h。
// core/ 代码永不 include 本头。
// CONFIG_HANDLE（config2.h）仅 plugin.cpp 内部使用（g_config_handle 为 file‑scope static），
// 故不在此暴露。
// ===================================================================

#include <windows.h>
#include "plugin2.h"
#include "logger2.h"

struct HostContext {
    EDIT_HANDLE* edit_handle = nullptr;
    LOG_HANDLE*  logger      = nullptr;
    HINSTANCE    dll_hinst   = nullptr;
};