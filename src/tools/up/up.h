#pragma once
#include "host/host_context.h"

// 注册 [AutoZWJ-UP] 剪贴板导入功能：
// 向右键菜单注册表登记本功能，并在启用状态下注册"空白处右键 → 导入 REAPER 剪贴板"菜单。
// 需在 plugin.cpp 的 RegisterPlugin 中调用。
void up_register_menu(HOST_APP_TABLE* host);
