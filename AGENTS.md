# AviUtl2 SDK & CLI 参考

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
- `get_object_alias()` — 获取 alias 格式的物件数据
- `set_object_item_value()` — 向效果参数写入 movement 字符串
- `get_object_item_value()` — 读取效果参数值
- `call_edit_section_param()` — 将时间轴编辑包装在 Undo 条目中
- `set_object_name()` / `get_object_name()` — 物件名称读写
- `enum_effect_item()` — 枚举物件上的效果
- `set_grid_bpm()` — 同步 AviUtl2 网格 BPM
- `PROJECT_FILE` — 键值对形式的工程级持久化存储

---

## 构建环境

### 推荐工具链
**MinGW-w64 (g++)**
- 使用 g++ 15.2.0 或更新版本
- 构建自包含的静态 DLL

### 构建命令

```powershell
# CMake（推荐，全量构建）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles"
cmake --build build --config Debug

# 使用 au2-cli（部署到 AviUtl2）
.\tools\au2-cli.exe dev
```

### 备选工具链
Visual Studio 2022 with Desktop C++ workload

---

## RPP 文件格式

- 纯文本，基于 chunk：`<NAME> ... >`
- Item 属性：POSITION, LENGTH, LOOP, PLAYRATE, SOFFS
- 源类型：VIDEO, WAVE, MP3, FLAC, VORBIS, MIDI

---

## 字符串编码

- `.object` 文件：UTF-8
- 插件 API：名称用宽字符串 (LPCWSTR)，alias 数据用 UTF-8 (LPCSTR)
- PROJECT_FILE 键/值：UTF-8

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

## 健壮性策略

### 日志系统
- 运行时错误对用户静默，不弹窗、不打断播放
- 所有错误只写入宿主控制台，使用统一前缀，默认只打印 Error
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
