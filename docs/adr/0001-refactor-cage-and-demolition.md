# 重构：隔离生成引擎 + 拆除 plugin.cpp + 修路去重

## 背景

`plugin.cpp` 是 1192 行的混装文件，塞了 5 类关注点（编码工具 / 插件生命周期 / 场景同步+注册 / 模板链字符串操作 / 生成引擎）。生成引擎（`on_generate_from_imgui` 及其 ~370 行循环 + `build_item_vars`/`evaluate_bakes_for_item`/`assign_layer_impl` 等）是 AI 生成、最难懂的核心逻辑。重构目标不是"代码变漂亮"，而是"一小时内能加一个简单功能"。

## 决策

### 1. 隔离：生成引擎关成纯数据黑盒

- 新建 `src/generation/generation.{h,cpp}`。入口 `std::vector<GeneratedObject> generate_object_specs(const GenInput&)`。
- **输入** `GenInput`（const 引用打包）：objdict、tracks、config、template_pool（含已提取的 `.chain`）、bakes_per_tpl、presets_per_tpl、scene、seed、shuffled_order、base_layer、logger。
- **输出** `GeneratedObject { int layer; int sf; int ef; std::string alias_chain; std::wstring name; }`——携带野兽自己拼好的整块 `[0]\nlayer=...\nframe=...\n...chain` 文本。
- **执行上下文不变**：引擎仍在 `call_edit_section_param` 的 lambda 内被调用，`update_scene_from_edit(edit)` 先刷新场景。把计算挪出 edit-section 锁属性能优化，被禁。
- **唯一逻辑触碰**：循环末尾两行 SDK 调用（`create_object_from_alias` L1006、`set_object_name` L1018）改为 `specs.push_back({...})`；写入由 lambda 内薄适配器完成。循环/算法/层分配/翻转/bake 求值零改动。
- scene 由隐式全局改为显式输入：`build_item_vars` 调用点 `g_scene_info`→`in.scene`（1 行；该函数本就收 `const SceneInfo&`）。
- 引擎保留 `logger` 入参用于 bake 求值失败告警（容忍的副作用，符合 AGENTS.md 错误策略）。

### 2. 拆除：plugin.cpp 函数分类外迁

- **进引擎做私有 static**（只被生成循环调用）：`apply_template_to_item`、`inject_param_bakes`、`inject_presets`、`inject_single_param`、`inject_motion_param`、`build_flip_block`、`build_item_vars`、`build_item_text_vars`、`evaluate_bakes_for_item`、`assign_layer_impl`、`ItemInterval`、`generate_shuffled_order`、`hash_string`、`pick_template_index`。
- **外移共享链模块** `src/chain/template_chain.{h,cpp}`（UI + on_open_config 受众）：`extract_template_chain`、`parse_effect_chain`。
- **外移独立 codec 模块** `src/codec/codec.{h,cpp}`：`utf8_to_wide`/`wide_to_utf8`/`cp932_to_utf8`/`maybe_cp932_to_utf8`/`is_valid_utf8`（后者去 static）。
- **留在 plugin.cpp 作精简入口**：生命周期入口、`RegisterPlugin`、`on_select_project`/`on_open_config`/`on_file_drop`、`update_scene_from_edit`、`sync_scene_info`。

### 3. 修路：去重唯一路径

- `is_valid_utf8` 因曾为 `static` 导致 `ui_components.cpp` 复制了 `is_valid_utf8_bytes`。codec 模块化后 `is_valid_utf8` 非静态公开，删除 `ui_components` 的副本，统一一条路径。

## 故意推迟（拒绝完美主义）

以下为已知坏味，本次不动，诚实记录：

- **`GhostSmallButton`/`GhostButton` 样式三重复制**（effect_chain_editor.cpp + ui_components.cpp）：去重需建共享 UI helper header，触及 UI 文件，擦边"禁止重构UI"。低价值。
- **`plugin.h` god-header**（结构体+全局+编码+SDK+生命周期挤一处）：拖慢编译但不阻塞加功能；拆分触及全部 include 站点（含 UI），大且险。
- **`g_template_aliases` 平行冗余于 `g_template_pool`**：数据冗余，但消除需改 UI 数据访问且算法相邻，非调用路径问题。
- **两个场景同步函数**（`update_scene_from_edit` vs `sync_scene_info`）：经核验非"多路径坏味"——前者读 `edit->info`（SDK 回调内有 `EDIT_SECTION*`），后者读 `g_edit_handle->get_edit_info()`（渲染循环冷启动兜底，无 `EDIT_SECTION*`）。上下文不同，两入口为上下文所必需，无可合并的单一路径。

## 取舍说明

- 选"纯数据黑盒"而非"原样副作用黑盒"：后者最贴"别碰算法"但输出是隐性副作用，不利于一小时内定位与改动；前者仅在写入点局部编辑（非算法改动），换取可推理性。
- 选"独立 codec 模块"而非"留 plugin.cpp+plugin.h"：callers 仍需 plugin.h 取结构体，include 耦合不改善，但结构体与编码分属不同 header、plugin.cpp/h 同步瘦身，分类更纯。
