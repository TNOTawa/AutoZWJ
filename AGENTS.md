# AutoZWJ — AviUtl2 插件开发参考

> 项目名称：AutoZWJ
> 类型：`.aux2` 泛用插件
> 功能：将 REAPER `.rpp` / MIDI `.mid` 工程以现有物件为模板批量导入 AviUtl2 时间轴

---

## 项目结构

```
src/
├── plugin.h/cpp                    # 插件入口、回调注册、模板生成逻辑
├── rpp/rpp_parser.h/cpp            # REAPER .rpp 解析器
├── midi/midi_parser.h/cpp          # SMF .mid 解析器
├── exo/object_generator.h/cpp      # .object alias 生成（已停用，现走模板流程）
├── effect/effect_dict.h/cpp        # ExEdit2 效果参数字典
└── ui/
    ├── file_picker.h/cpp           # 原生 Win32 OPENFILENAME 文件选择
    ├── project_state.cpp           # 解析调度、轨道选择辅助、持久化
    ├── imgui_window.h/cpp          # Dear ImGui + DX11 配置窗口
    └── ui_components.h/cpp         # 轨道树、配置面板、执行按钮
```

---

## 核心设计决策

| 决策 | 说明 |
|------|------|
| **模板驱动生成** | 以时间轴已有物件为样式模板，继承其效果链，仅替换文件路径/再生位置/速度等参数 |
| **两阶段分离** | ① 右键空白→选择工程（仅解析）；② 右键物件→配置导入→ImGui窗口内执行生成 |
| **空轨过滤** | RPP 解析后 `erase(remove_if)` 移除 `count<=0` 的空轨，轨道编号保留原 REAPER 编号 |
| **层分配** | 参考 RPPtoEXO 原版的贪婪首次适配，float `bf` vs float `obj_fp` 比较后取整输出 |
| **帧计算** | `sf = round(pos*fps+1.0)`, `ef = round(bf)`，AviUtl2 帧从 1 开始 |
| **ImGui 窗口** | 独立 `WS_OVERLAPPEDWINDOW` + `SetTimer(16ms)` 驱动渲染，不注册为子窗口 |

## 模板模式的关键流程

```
1. 右键空白 → "选择音频工程..." → 解析 RPP/MIDI → 存入 g_project_state
2. 时间轴选中模板物件
3. 右键物件 → "配置导入..." → 读取模板 alias → 提取 [0.0] 开始的 effect chain
4. ImGui 窗口内配置轨道选择、翻转、填充间隙等
5. 点击"执行导入生成" → call_edit_section_param → 逐 item 创建物件
```

---

## AviUtl2 SDK & CLI 参考

## AviUtl2 CLI

- **位置**: `./tools/au2-cli.exe`
- **文档**: `./tools/au2-cli.md`
- **要求**: 启用 Windows 开发者模式（用于 symlink）
- **命令**:
  - `au2 init` — 创建 `aviutl2.toml` 配置
  - `au2 prepare` — 配置完整开发环境
  - `au2 dev` / `au2 develop` — 构建并部署开发版本
  - `au2 release` — 构建发布包

---

## AviUtl2 SDK

### 位置
`./aviutl2_sdk/`

### 主要头文件
- `plugin2.h` — 汎用プラグイン (general purpose plugin, `.aux2`)
- `filter2.h` — Filter plugin (`.auf2`)
- `module2.h` — Script module (`.mod2`)
- `config2.h` — Config API
- `logger2.h` — Logging API

### 插件类型
| 扩展名 | 类型 | 说明 |
|--------|------|------|
| `.aux2` | General purpose plugin | 主插件类型 |
| `.mod2` | Script module | 脚本模块 |
| `.auf2` | Filter plugin | 滤镜插件 |
| `.aui2` | Input plugin | 输入插件 |
| `.auo2` | Output plugin | 输出插件 |

### 关键 SDK API
- `RegisterPlugin(HOST_APP_TABLE* host)` — **必须**的入口点
- `create_object_from_alias()` — 从 .object 格式创建物件
- `get_object_alias()` — 获取 alias 格式的物件数据（返回 `[Object]`/`[Object.N]` section 格式）
- `set_object_item_value()` — 向效果参数写入 movement 字符串
- `get_object_item_value()` — 读取效果参数值
- `call_edit_section_param()` — 将时间轴编辑包装在 Undo 条目中
- `set_object_name()` / `get_object_name()` — 物件名称读写
- `enum_effect_item()` — 枚举物件上的效果
- `register_layer_menu()` / `register_object_menu()` — 右键菜单注册
- `register_file_drop_handler()` — 文件拖放注册
- `get_edit_info()` — 获取场景信息（FPS、分辨率、帧号等）
- `get_object_layer_frame()` — 获取物件所在层和帧位置
- `get_selected_object()` / `get_selected_object_num()` — 获取选中物件
- `set_grid_bpm()` — 同步 AviUtl2 网格 BPM
- `PROJECT_FILE` — 键值对形式的工程级持久化存储

---

## 已踩坑 & 禁忌

### 致命错误
1. **alias section 格式**：`get_object_alias()` 返回 `[Object]`/`[Object.N]`，`create_object_from_alias()` 输入可接受 `[0]`/`[0.N]`。两者不可混用——模板提取时必须将 `[Object.N]` 归一化为 `[0.N]`。
2. **帧从 1 开始**：AviUtl2 帧编号从 1 开始。`pos_fps + 1.0` 而非 `pos_fps`。
3. **层占用判定**：用 float `bf`（未取整）比较 float `obj_fp` 后，再用 int 输出。层检查早于取整。
4. **`call_edit_section_param` 不可嵌套**：在已有的 EDIT_SECTION 回调内再次调用会崩溃。
5. **lambda with capture 不能传 C 函数指针**：需用 `call_edit_section_param(state, ...)` 传参数，lambda 必须是无捕获的 `[](void* param, EDIT_SECTION* edit)`。
6. **`get_selected_object()` 在回调外失效**：物件选中仅在菜单回调时有效，应在 `on_open_config` 时立即读取模板 alias 并缓存。
7. **`get_project_file()` 参数不可传 `nullptr`**：SDK 签名要求传 `EDIT_HANDLE*`，实际传 `g_edit_handle`。传 `nullptr` 会导致宿主内部解引用崩溃。
8. **跨工程数据隔离（关键）**：AviUtl2 切换工程时插件 DLL 通常不被卸载，所有全局变量（`g_project_state`、`g_template_pool` 等）会残留上一个工程的数据。任何 EDIT_SECTION 回调入口都必须先调用 `load_project_state_from_project_file(edit)`，且该函数开头必须执行 `g_project_state = ProjectState{}` 将工程相关字段全部重置，再从当前工程的 `PROJECT_FILE` 重新加载。禁止任何懒加载/缓存机制绕过此重置。

### UI 规则
- 标签统一为中文，不加英文括号注释
- `register_layer_menu` / `register_object_menu` 分别对应空白处和有物件的右键菜单
- ImGui 窗口独立消息循环：`SetTimer(hwnd, 1, 16, nullptr)` + `WM_TIMER` → `render_frame()`
- 首次渲染强制调用一次 `render_frame()` 避免白屏

---

## 构建环境

### 推荐工具链
**MinGW-w64 (g++)**
- 使用 g++ 15.2.0 或更新版本
- 构建自包含的静态 DLL

### 构建命令

```powershell
# CMake（Release）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build --config Release

# 产物: build/AutoZWJ.aux2
```

### 备选工具链
Visual Studio 2022 with Desktop C++ workload

### ImGui 下载（网络受限时的本地导入）

```powershell
$base = "https://raw.githubusercontent.com/ocornut/imgui/docking"
$dest = "src/thirdparty/imgui"
$backends = "$dest/backends"
New-Item -ItemType Directory -Force -Path $dest, $backends | Out-Null
$files = @("imgui.h","imgui.cpp","imgui_draw.cpp","imgui_tables.cpp","imgui_widgets.cpp",
           "imgui_internal.h","imconfig.h","imstb_rectpack.h","imstb_textedit.h","imstb_truetype.h",
           "backends/imgui_impl_win32.h","backends/imgui_impl_win32.cpp",
           "backends/imgui_impl_dx11.h","backends/imgui_impl_dx11.cpp")
foreach ($f in $files) {
    Invoke-WebRequest -Uri "$base/$f" -OutFile "$dest/$f" -TimeoutSec 30 -UseBasicParsing
}
```

---

## RPP 文件格式

- 纯文本，基于 chunk：`<NAME> ... >`
- Item 属性：POSITION, LENGTH, LOOP, PLAYRATE, SOFFS
- 源类型：VIDEO, WAVE, MP3, FLAC, VORBIS, MIDI
- 空轨：`<TRACK>` 存在但无 `<ITEM>` —— 解析后须过滤，避免其 TrackNode 污染索引

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
3. **风险前置** — 架构未知因素尽早用最少代码验证（原型先行）
4. **旧路径存活到新路径切过去那天** — 新版就绪后再删除旧代码
5. **MVP 优先** — 尽早交付可用产出

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
- 所有错误只写入宿主控制台，使用统一前缀 `AutoZWJ:`，默认只打印 Error
- 同一错误短时间内做节流，避免逐帧重复报错刷屏
- 脚本端遇到错误返回当前参数默认值或安全值

### 存储损坏与格式不兼容
- 读取序列化配置时校验结构完整性
- `schema` 字段用于版本兼容——高版本数据不写回、不自动修复
- 缺失或损坏的条目 → 回退到默认配置

### 写回流程
1. 先调用 SDK API 写入数据到目标位置
2. 再将序列化数据写入持久化存储
3. 写入失败时不修改缓存，记录错误，物件保持原状
