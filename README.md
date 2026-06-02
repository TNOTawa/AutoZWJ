# AutoZWJ

> RPP/MIDI → AviUtl2 物件导入插件

将 REAPER 工程文件（`.rpp`）或标准 MIDI 文件（`.mid`）中的音视频素材，以用户在 AviUtl2 中选定的物件为模板，批量生成到时间轴上。

---

## 功能

- 解析 `.rpp`（REAPER 工程）和 `.mid`（标准 MIDI）文件
- 以时间轴上已有物件为**样式模板**，继承其效果链、参数设定
- **多源映射** —— 选中多个模板物件，按策略（顺序轮替 / 随机抽选 / 和弦映射）分配
- **效果链编辑器** —— 只读展示模板效果链，勾选参数 bake + 设置目标值
- **脚本变量系统** —— `$note.velocity$ / 127 * 200` 表达式驱动 bake 值
- **动态预设** —— 翻转等预设可视化，拖拽调整插入位置
- 在模板下方自动分配图层，紧凑排列
- 支持轨道树形展示，含全选 / 取消全选 / 反选
- 交替翻转（左右 / 上下 / 顺时针旋转 / 逆时针旋转）
- 物件时长控制（与音符对齐 / 拉伸到下一音符 / 固定帧数 / 仅在间隙生成）
- 多音符策略（全部独立 / 仅取第N轨 / 仅取倒数第N轨）
- 偶数项换行（奇偶物件分离图层）
- 所有参数自动从当前场景读取（FPS、分辨率）

---

## 安装

将 `AutoZWJ.aux2` 放入 AviUtl2 的 `Plugin` 目录，启动 AviUtl2 即可加载。

---

## 使用流程

1. **加载工程**  
   时间轴空白处右键 → **选择音频工程...** → 选取 `.rpp` 或 `.mid` 文件

2. **选择模板**  
   在时间轴上选中一个或多个物件，作为输出样式模板（效果链、滤镜等将被继承）

3. **配置参数**  
   右键已选物件 → **配置导入...** → 在弹出窗口中调整：

   **基础设置**
   | 选项 | 说明 |
   |------|------|
   | 交替翻转 | 启用物件翻转，选择翻转方向（4 种模式） |
   | 物件时长 | 与音符对齐 / 拉伸到下一音符 / 固定帧数 / 仅在间隙生成 |
   | 多音符策略 | 全部独立 / 仅取第N轨 / 仅取倒数第N轨 |
   | 偶数项换行 | 偶数索引物件自动下移一层 |

   **多模板映射**（选中 ≥2 个模板物件时自动启用）
   | 策略 | 说明 |
   |------|------|
   | 顺序轮替 | 模板按顺序循环分配给物件（支持顺序/倒序/洗牌） |
   | 随机抽选 | 每个物件随机选择模板（确定性，同工程结果一致） |
   | 和弦映射 | 和弦中不同位置的音符使用不同模板 |

   **效果链编辑器**（点击右侧 `效果链编辑` 按钮）
   - 只读展示每个模板物件的完整效果链
   - 勾选参数进入 bake 模式 → 设固定值 / `$变量名$` 映射 / 表达式求值
   - 可用变量：`$note.pitch$` / `$note.velocity$` / `$chord.index$` 等 25+ 种
   - 动态预设（翻转等）可视化并拖拽调整插入位置

4. **执行生成**  
   点击窗口底部 **执行导入生成** 按钮，物件将创建在模板下方

---

## 构建

### 依赖

- MinGW-w64 (g++ 15.2+)
- CMake 3.20+
- Dear ImGui (docking branch，置于 `src/thirdparty/imgui/`)
- DirectX 11 SDK（Windows 自带）

### 构建命令

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build --config Release
```

产物为 `build/AutoZWJ.aux2`。

---

## 项目结构

```
src/
├── plugin.h/cpp                    # 插件入口、回调注册、生成引擎、层分配、变量绑定
├── rpp/rpp_parser.h/cpp            # REAPER .rpp 解析器
├── midi/midi_parser.h/cpp          # SMF .mid 解析器
├── exo/object_generator.h/cpp      # .object alias 生成（已停用）
├── effect/effect_dict.h/cpp        # ExEdit2 效果参数字典（名称映射）
├── script/
│   ├── expr_evaluator.h            # 递归下降表达式求值器（仅头文件）
│   └── variable_subst.h            # $var$ 变量替换器（仅头文件）
├── ui/
│   ├── file_picker.h/cpp           # 原生 Win32 文件选择对话框
│   ├── project_state.cpp           # 解析调度、持久化、工程历史管理
│   ├── imgui_window.h/cpp          # ImGui + DX11 配置窗口
│   ├── ui_config.h                 # UI 风格常量
│   ├── ui_components.h/cpp         # 轨道树、导入/配置页面、参数面板
│   └── effect_chain_editor.h/cpp   # 效果链编辑器面板（bake + 变量 + 预设拖拽）
└── thirdparty/imgui/               # Dear ImGui 源码
```

---

## 参考

- [RPPtoEXO ver2.0](https://github.com/Garech-mas/RPPtoEXO-ver2.0)
- [OtomadHelper](https://github.com/otomad/OtomadHelper)
- [om_midi](https://github.com/otomad/om_midi)
- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)

---

## 许可

MIT License
