# 构建指南

## 依赖

| 工具 | 版本 | 说明 |
|------|------|------|
| MinGW-w64 | g++ 15.2+ (需支持 C++20) | 构建自包含的静态 DLL |
| CMake | 3.20+ | 构建系统 |
| Dear ImGui | docking branch | 置于 `src/thirdparty/imgui/` |
| DirectX 11 SDK | — | Windows 自带 |

## 构建命令

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build
```

> MinGW Makefiles 为**单配置生成器**，构建类型由首次 `-DCMAKE_BUILD_TYPE` 锁定到缓存。若需切换 Debug/Release，先删除 `build/CMakeCache.txt` 再重新配置。

产物为 `build/AutoZWJ.aux2`。

---

## 项目结构

```
src/
├── plugin.h/cpp                    # 插件入口、回调注册、生成引擎、层分配、变量绑定
├── i18n/
│   ├── i18n.h/cpp                  # 多语言运行时（tr/tr_str/tr_fmt）
│   └── lang/
│       ├── zh-CN.ini               # 简体中文语言资源（key = 中文原文）
│       ├── en.ini                   # 英语语言资源
│       └── ja.ini                  # 日本語语言资源
├── parsers/
│   ├── rpp/rpp_parser.h/cpp        # REAPER .rpp 解析器
│   ├── midi/midi_parser.h/cpp      # SMF .mid 解析器
│   ├── lrc/lrc_parser.h/cpp        # LRC 歌词解析器
│   └── tempo_convert.h             # tempo_map → BPM_INFO[] 共享转换（仅头文件）
├── codec/codec.h/cpp               # 字符编码转换（utf8/wide/cp932）
├── chain/template_chain.h/cpp      # 模板效果链提取与解析
├── generation/generation.h/cpp     # 物件生成引擎（纯数据黑盒）
├── exo/object_generator.h/cpp      # .object alias 生成（旧路径，已停用）
├── effect/effect_dict.h/cpp        # ExEdit2 效果参数字典（名称映射）
├── script/
│   ├── expr_evaluator.h            # 递归下降表达式求值器（仅头文件）
│   └── variable_subst.h            # $var$ 变量替换器（仅头文件）
├── ui/
│   ├── file_picker.h/cpp           # 原生 Win32 OPENFILENAME 文件选择
│   ├── project_state.cpp           # 解析调度、持久化、工程历史管理
│   ├── imgui_window.h/cpp          # Dear ImGui + DX11 独立窗口
│   ├── ui_config.h                 # UI 风格常量（字体、色彩系统、主题注入）
│   ├── ui_components.h/cpp         # 轨道树、导入/配置页面、参数面板
│   └── effect_chain_editor.h/cpp   # 效果链编辑器面板（参数 bake + 变量映射 + 预设拖拽）
├── tools/
│   └── tempo/tempo_apply.h/cpp     # BPM 网格同步工具
└── thirdparty/imgui/               # Dear ImGui docking 分支
```

---

## 设计要点

### 模板驱动生成

以时间轴已有物件为样式模板，继承其效果链，仅替换再生位置/速度/循环等参数。这避免了为每种素材类型硬编码效果块，用户只需在时间轴上配置好一个"样板"物件即可。

### 两阶段分离

1. 右键空白 → 选择工程（仅解析）
2. 右键物件 → 配置导入 → ImGui 窗口内执行生成

这种分离让用户可以先确认工程解析结果，再决定如何生成。

### 空轨过滤

解析后保留空轨在树中（灰色可选），但生成时静默跳过。这保持了轨道结构的完整性，同时不影响输出。

### 字符串编码

| 文件/接口 | 编码 |
|-----------|------|
| `.object` 文件 | UTF-8 |
| 插件 API 名称 | 宽字符串 (LPCWSTR) |
| 插件 API alias | UTF-8 (LPCSTR) |
| PROJECT_FILE 键/值 | UTF-8 |
| RPP 文件 | cp932 或 UTF-8（自动检测转换） |

### AviUtl2 Round-trip 行为

`create_object_from_alias()` → `get_object_alias()` 往返会剥离非标准 section。因此自定义数据不能存在 alias 内，必须外移到 `PROJECT_FILE`。

### C++20

项目使用 C++20 标准，主要用到了 `std::format`（`tr_fmt` 模板函数）和 `constexpr` 增强（色彩系统）。语言文件通过 CMake `configure_file()` 在构建时嵌入，无需运行时文件 I/O。

---

## 参考

- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)
