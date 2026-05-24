# AutoZWJ

> RPP/MIDI → AviUtl2 物件导入插件  

将 REAPER 工程文件（`.rpp`）或标准 MIDI 文件（`.mid`）中的音视频素材，以用户在 AviUtl2 中选定的物件为模板，批量生成到时间轴上。

---

## 功能

- 解析 `.rpp`（REAPER 工程）和 `.mid`（标准 MIDI）文件
- 以时间轴上已有物件为**样式模板**，继承其效果链、参数设定
- 在模板下方自动分配图层，紧凑排列
- 支持轨道树形展示，含全选 / 取消全选 / 反选
- 交替翻转（左右 / 上下 / 顺时针旋转 / 逆时针旋转）
- 偶数项换行（奇偶物件分离图层）
- 填充间隙（消除帧间空白）
- 帧取整可选四舍五入或向上取整
- 所有参数自动从当前场景读取（FPS、分辨率）

---

## 安装

将 `AutoZWJ.aux2` 放入 AviUtl2 的 `Plugin` 目录，启动 AviUtl2 即可加载。

---

## 使用流程

1. **加载工程**  
   时间轴空白处右键 → **选择音频工程...** → 选取 `.rpp` 或 `.mid` 文件

2. **选择模板**  
   在时间轴上选中一个物件，作为输出样式模板（效果链、滤镜等将被继承）

3. **配置参数**  
   右键已选物件 → **配置导入...** → 在弹出窗口中调整：

   | 选项 | 说明 |
   |------|------|
   | 交替翻转 | 启用物件翻转，选择翻转方向 |
   | 翻转方向 | 左右翻转、上下翻转、顺时针旋转、逆时针旋转 |
   | 偶数项换行 | 偶数索引物件自动下移一层 |
   | 无节拍同步 | 忽略素材时长循环对齐 |
   | 填充间隙 | 自动延长物件消除帧间空白 |
   | 向上取整帧 | 帧计算使用 `ceil` 替代 `round` |

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

产物为 `build/RPPinEXO.aux2`，重命名为 `AutoZWJ.aux2` 后使用。

### 下载 ImGui（如未自动获取）

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

## 项目结构

```
src/
├── plugin.h/cpp                    # 插件入口、回调注册、生成逻辑
├── rpp/rpp_parser.h/cpp            # REAPER .rpp 解析器
├── midi/midi_parser.h/cpp          # SMF .mid 解析器
├── exo/object_generator.h/cpp      # .object alias 生成
├── effect/effect_dict.h/cpp        # ExEdit2 效果参数字典
├── ui/
│   ├── file_picker.h/cpp           # 原生 Win32 文件选择对话框
│   ├── project_state.cpp           # 解析调度、轨道选择辅助
│   ├── imgui_window.h/cpp          # ImGui + DX11 配置窗口
│   └── ui_components.h/cpp         # 轨道树、配置面板、按钮
└── thirdparty/imgui/               # Dear ImGui 源码
```

---

## 参考

- RPPtoEXO ver2.11 — 原 Python 版
- AviUtl2 Plugin SDK — 官方 SDK 文档与示例
- Dear ImGui — GUI 库

---

## 许可

MIT License
