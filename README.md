[English](docs/en/) | [日本語](docs/ja/)

# AutoZWJ

> RPP / MIDI → AviUtl2 物件批量导入插件

将 REAPER 工程文件（`.rpp`）、标准 MIDI 文件（`.mid`）或 LRC 歌词文件（`.lrc`）中的素材，以用户在 AviUtl2 中选定的物件为模板，批量生成到时间轴上。

## 功能概述

- 解析 REAPER `.rpp`、标准 MIDI `.mid`、LRC `.lrc` 等工程文件
- 以时间轴上已有物件为**样式模板**，继承其效果链、参数设定
- **多源映射** —— 选中多个模板物件，按策略（顺序轮替 / 随机抽选 / 和弦映射 / 动画序列）分配
- **效果链编辑器** —— 只读展示模板效果链，勾选参数 bake + 设置目标值 / 变量映射 / 表达式求值
- **脚本变量系统** —— `$note.velocity$ / 127 * 200` 等表达式驱动 bake 值
- **BPM 网格同步** —— 将 MIDI/RPP 的 tempo map 写入 AviUtl2 的 BPM 网格
- **国际化支持** —— 界面支持简体中文 / English / 日本語，自动检测宿主 UI 语言
- 在模板下方自动分配图层，紧凑排列
- 交替翻转、物件时长控制、多音符策略、偶数项换行
- 所有参数自动从当前场景读取（FPS、分辨率），并跨会话持久化

## 安装

将 `AutoZWJ.aux2` 放入 AviUtl2 的 `Plugin` 目录，启动 AviUtl2 即可加载。

## 快速上手

1. **开始使用**  
   在时间轴上**选中任意一个物件**，右键 → **配置导入...**。若时间轴上还没有物件，可先任意放置一个素材作为模板，或直接跳至第 2 步拖入工程文件

2. **导入工程**  
   若尚未导入工程，插件窗口会自动进入工程导入页：将 `.rpp` / `.mid` 文件拖入 AviUtl2 窗口，或在导入页选择 REAPER 最近文件 / 浏览...；随后勾选要生成的轨道，点击**确认导入**

3. **配置并生成**  
   在配置页调整参数 → 点击**确定**或**应用**

详细教程与功能说明请参阅 [文档](docs/zh/)。

## 构建

需要 MinGW-w64 (g++ 15.2+)、CMake 3.20+。AviUtl2 SDK 与 Dear ImGui 以 git 子模块管理，首次克隆后先初始化：

```powershell
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles"
cmake --build build
```

产物为 `build/AutoZWJ.aux2`。正式发版由 GitHub Actions 在推送 `v*` tag 时自动构建 Release 并发布，详见 [docs/zh/build.md](docs/zh/build.md)。

## 参考

- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [RPPtoEXO ver2.0](https://github.com/Garech-mas/RPPtoEXO-ver2.0)
- [OtomadHelper](https://github.com/otomad/OtomadHelper)
- [om_midi](https://github.com/otomad/om_midi)
- [import_midi_tempos.aux2](https://github.com/sevenc-nanashi/import_midi_tempos.aux2)
- [UltraPaste](https://github.com/zzzzzz9125/UltraPaste)

## 许可

MIT License
