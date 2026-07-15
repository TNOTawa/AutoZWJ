# AutoZWJ — AviUtl2 插件开发参考

> 项目名称：AutoZWJ
> 类型：`.aux2` 泛用插件
> 功能：将 REAPER `.rpp` / MIDI `.mid` 工程以现有物件为模板批量导入 AviUtl2 时间轴
> 状态：核心功能已闭环（Phase 1~7 + 脚本系统完成 + BPM 网格工具）

---

## 项目结构

```
src/
├── plugin.h/cpp                    # 插件入口、回调注册、生成引擎、层分配、变量绑定
├── parsers/
│   ├── rpp/rpp_parser.h/cpp        # REAPER .rpp 解析器
│   ├── midi/midi_parser.h/cpp      # SMF .mid 解析器
│   ├── lrc/lrc_parser.h/cpp        # LRC 歌词解析器
│   └── tempo_convert.h             # tempo_map → BPM_INFO[] 共享转换（仅头文件）
├── codec/codec.h/cpp               # 字符编码转换（utf8/wide/cp932）
├── chain/template_chain.h/cpp      # 模板效果链提取与解析
├── generation/generation.h/cpp     # 物件生成引擎（纯数据黑盒）
├── exo/object_generator.h/cpp      # .object alias 生成（已停用，现走模板流程）
├── effect/effect_dict.h/cpp        # ExEdit2 效果参数字典（名称映射）
├── script/
│   ├── expr_evaluator.h            # 递归下降表达式求值器（仅头文件）
│   └── variable_subst.h            # $var$ 变量替换器（仅头文件）
├── ui/
│   ├── file_picker.h/cpp           # 原生 Win32 OPENFILENAME 文件选择
│   ├── project_state.cpp           # 解析调度、持久化、工程历史管理
│   ├── imgui_window.h/cpp          # Dear ImGui + DX11 独立窗口
│   ├── ui_config.h                 # UI 风格常量（字体、颜色、间距）
│   ├── ui_components.h/cpp         # 轨道树、导入/配置页面、参数面板
│   └── effect_chain_editor.h/cpp   # 效果链编辑器面板（参数 bake + 变量映射 + 预设拖拽）
├── tools/
│   └── tempo/tempo_apply.h/cpp     # BPM 网格同步工具（增强非必要功能）
└── thirdparty/imgui/               # Dear ImGui docking 分支
```

---

## 核心设计决策

| 决策 | 说明 |
|------|------|
| **模板驱动生成** | 以时间轴已有物件为样式模板，继承其效果链，仅替换再生位置/速度/循环等参数 |
| **两阶段分离** | ① 右键空白→选择工程（仅解析）；② 右键物件→配置导入→ImGui窗口内执行生成 |
| **空轨过滤** | 解析后保留空轨在树中（灰色可选），但生成时静默跳过 |
| **层分配** | 参考 RPPtoEXO 原版的贪婪首次适配，float `bf` vs float `obj_fp` 比较后取整输出 |
| **帧计算** | `sf = round(pos*fps+1.0)`, `ef = round(bf)`，AviUtl2 帧从 1 开始 |
| **ImGui 窗口** | 独立 `WS_OVERLAPPEDWINDOW` + `SetTimer(16ms)` 驱动渲染 |
| **多源映射** | 支持多模板物件选中，三种策略分配模板：顺序轮替 / 随机抽选 / 和弦映射 |
| **效果链编辑器** | 三面板抽屉式布局，只读展示模板效果链，勾选参数 bake + 设置目标值 |
| **脚本变量系统** | `$note.velocity$ / 127 * 200` 表达式驱动 bake 值，`ExprEvaluator` 递归下降求值 |
| **动态预设** | 翻转等预设可视化并在效果链面板中拖拽调整插入位置 |
| **BPM 网格工具** | 从 MIDI/RPP 提取 tempo map，经 `tempo_map_to_bpm_info()` 秒域统一转换后写 `set_grid_bpm_list`；按钮主动触发，不持久化 |

## 模板模式的关键流程

```
1. 右键空白 → "选择音频工程..." → 解析 RPP/MIDI → 存入 g_project_state
2. 时间轴选中一个或多个模板物件
3. 右键物件 → "配置导入..." → 读取模板 alias → 提取 [0.0] 开始的 effect chain
4. ImGui 窗口内配置：
   - 轨道树选择、场景信息
   - 翻转/间隙/物件时长/多音符策略
   - 多模板映射策略（顺序轮替/随机/和弦）
   - 效果链编辑器：参数 bake + 变量映射 + 预设拖拽
5. 点击"执行导入生成"
6. 生成循环：pick_template_index → build_item_vars → evaluate_bakes → inject_param_bakes → inject_presets → create_object_from_alias
```

---

## 已踩坑 & 禁忌

### 致命错误
1. **alias section 格式**：`get_object_alias()` 返回 `[Object]`/`[Object.N]`，`create_object_from_alias()` 输入可接受 `[0]`/`[0.N]`。模板提取时必须将 `[Object.N]` 归一化为 `[0.N]`。
2. **帧从 1 开始**：AviUtl2 帧编号从 1 开始。
3. **层占用判定**：用 float `bf`（未取整）比较 float `obj_fp` 后，再用 int 输出。
4. **`call_edit_section_param` 不可嵌套**。
5. **lambda with capture 不能传 C 函数指针**：需用无捕获 lambda + `call_edit_section_param(state, ...)` 传参。
6. **`get_selected_object()` 在回调外失效**：在 `on_open_config` 时立即读取模板 alias 并缓存。
7. **`get_project_file()` 参数不可传 `nullptr`**：传 `g_edit_handle`。
8. **跨工程数据隔离**：切换工程时全局变量不卸载。任何 EDIT_SECTION 回调入口必须先重置 `g_project_state = ProjectState{}` 再从 `PROJECT_FILE` 重新加载。
9. **模板 alias 中隐藏键**：`Group=1`、`Group2=1` 等 SDK 不报告的键，`parse_effect_chain()` 和 `inject_param_bakes()` 必须原样保留。
10. **运动参数格式**：`位置=起点,终点,方法,保留|曲线` 为逗号分隔复合值，编辑起点/终点时其余字段原样保留。

### UI 规则
- 标签统一为中文，不加英文括号注释
- `register_layer_menu` / `register_object_menu` 分别对应空白处和有物件的右键菜单
- ImGui 窗口独立消息循环：`SetTimer(hwnd, 1, 16, nullptr)` + `WM_TIMER` → `render_frame()`
- 首次渲染强制调用一次 `render_frame()` 避免白屏
- 效果链面板采用三列抽屉式布局，`ImLerp` 平滑动画

---

## 构建环境

### 推荐工具链
**MinGW-w64 (g++)**
- 使用 g++ 15.2.0 或更新版本
- 构建自包含的静态 DLL

### 构建命令

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build

# 产物: build/AutoZWJ.aux2
```

> MinGW Makefiles 为单配置生成器，构建类型由首次 `-DCMAKE_BUILD_TYPE` 锁定到缓存。若需切换 Debug/Release，先删除 `build/CMakeCache.txt` 再重新配置。

---

## RPP 文件格式

- 纯文本，基于 chunk：`<NAME> ... >`
- Item 属性：POSITION, LENGTH, LOOP, PLAYRATE, SOFFS
- 源类型：VIDEO, WAVE, MP3, FLAC, VORBIS, MIDI
- 空轨：`<TRACK>` 存在但无 `<ITEM>` —— 解析后在树中保留但生成时跳过
- Tempo：工程头 `TEMPO <bpm> <分子> <分母>` 提供初始 BPM/拍号；`<TEMPOENVEX>` 内 `PT <pos_sec> <bpm> <shape> ...` 行提供变速点（position 单位为秒）

---

## 字符串编码

- `.object` 文件：UTF-8
- 插件 API：名称用宽字符串 (LPCWSTR)，alias 数据用 UTF-8 (LPCSTR)
- PROJECT_FILE 键/值：UTF-8
- RPP 文件可能为 cp932（日文环境），需 `MultiByteToWideChar(932,...)` 转 UTF-8

---

## AviUtl2 Round-trip 行为

- `create_object_from_alias()` → `get_object_alias()` 往返会剥离非标准 section
- **结论**：自定义数据不能存在 alias 内，必须外移到 PROJECT_FILE

---

## 设计原则

1. **每 Phase 产出可测试物** — CLI 测试或宿主可观察行为
2. **依赖链先行** — 底层工具 → 中间逻辑 → 上层集成
3. **风险前置** — 架构未知因素尽早用最少代码验证
4. **旧路径存活到新路径切过去那天** — 新版就绪后再删除旧代码
5. **MVP 优先** — 尽早交付可用产出
6. **权限放开** — 不做预判性限制，用户自主判断值的合法性

---

## 开发铁律

> 以瞎猜接口为耻，以认真查询为荣。
> 以模糊执行为耻，以寻求确认为荣。
> 以臆想业务为耻，以人类确认为荣。
> 以创造接口为耻，以复用现有为荣。
> 以跳过验证为耻，以主动测试为荣。
> 以破坏架构为耻，以遵循规范为荣。
> 以假装理解为耻，以诚实无知为荣。
> 以盲目修改为耻，以谨慎重构为荣。

---

## 健壮性策略

### 日志系统
- 运行时错误对用户静默，不弹窗、不打断播放
- 所有错误只写入宿主控制台，使用统一前缀 `AutoZWJ:`
- 同一错误短时间内做节流，避免逐帧重复报错
- 表达式/变量求值失败时跳过该 bake，不中断生成

### 持久化
- `OutputConfig` 字段通过 `PROJECT_FILE` API 以 `rppinexo.*` 键名序列化
- 工程文件路径和最近文件历史（最多 10 条）独立持久化
- 跨工程切换通过 `load_project_state_from_project_file()` 重置 + 重载

### 写回流程
1. 先调用 SDK API 写入数据到目标位置
2. 再将序列化数据写入持久化存储
3. 写入失败时不修改缓存，记录错误，物件保持原状
