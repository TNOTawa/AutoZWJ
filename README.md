[English](docs/en/) | [日本語](docs/ja/)

# AutoZWJ

> RPP / MIDI → AviUtl2 物件批量导入插件

将 REAPER 工程文件（`.rpp`）或标准 MIDI 文件（`.mid`）中的素材，以用户在 AviUtl2 中选定的物件为模板，批量生成到时间轴上。

## 功能概述

- 解析 REAPER `.rpp`、标准 MIDI `.mid` 等工程文件（更多格式持续添加中）
- 以时间轴上已有物件为**样式模板**，继承其效果链、参数设定
- **多源映射** —— 选中多个模板物件，按策略（顺序轮替 / 随机抽选 / 和弦映射）分配
- **效果链编辑器** —— 只读展示模板效果链，勾选参数 bake + 设置目标值 / 变量映射 / 表达式求值
- **脚本变量系统** —— `$note.velocity$ / 127 * 200` 等表达式驱动 bake 值
- 在模板下方自动分配图层，紧凑排列
- 交替翻转、物件时长控制、多音符策略、偶数项换行
- 所有参数自动从当前场景读取（FPS、分辨率），并跨会话持久化

## 安装

将 `AutoZWJ.aux2` 放入 AviUtl2 的 `Plugin` 目录，启动 AviUtl2 即可加载。

## 快速上手

1. **加载工程**  
   时间轴空白处右键 → **选择音频工程...** → 选取 `.rpp` 或 `.mid` 文件

2. **选择模板**  
   在时间轴上选中一个或多个物件，作为输出样式模板（效果链、滤镜等将被继承）

3. **配置并生成**  
   右键已选物件 → **配置导入...** → 在弹出窗口中调整参数 → 点击**确定**或**应用**

详细教程与功能说明请参阅 [文档](docs/zh/)。

## 构建

需要 MinGW-w64 (g++ 15.2+)、CMake 3.20+、Dear ImGui docking branch。

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build
```

产物为 `build/AutoZWJ.aux2`。

详见 [docs/zh/build.md](docs/zh/build.md)。

## 参考

- [RPPtoEXO ver2.0](https://github.com/Garech-mas/RPPtoEXO-ver2.0)
- [OtomadHelper](https://github.com/otomad/OtomadHelper)
- [om_midi](https://github.com/otomad/om_midi)
- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)

## 许可

MIT License
