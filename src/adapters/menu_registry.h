#pragma once
#include <string>
#include <vector>

// 右键菜单注册项（全局配置，非工程级）。
// 菜单注册发生在 RegisterPlugin（插件加载时），SDK 无 unregister 能力，
// 因此开关状态持久化到 <app_data_dir>\AutoZWJ.menu.ini，重启 AviUtl2 后生效。
struct MenuEntry {
    std::string id;          // 唯一标识，同时作为 ini 键名（如 up.clipboard）
    std::string label_key;   // 翻译键（中文原文），显示时经 tr() 翻译
    bool enabled = true;
};

// 注册表内存态
std::vector<MenuEntry>& menu_registry_all();

// 登记一项（已存在则忽略）。启用状态优先取持久化值，其次取 default_enabled
void menu_registry_upsert(const std::string& id, const std::string& label_key, bool default_enabled);

// 修改开关状态并立即写盘
void menu_registry_set_enabled(const std::string& id, bool enabled);

// 从 <app_data_dir>\AutoZWJ.menu.ini 加载持久化状态（记住目录，供 save 使用）
void menu_registry_load(const std::wstring& app_data_dir);

// 将当前注册表状态写回 AutoZWJ.menu.ini（须先调用 load）
void menu_registry_save();
