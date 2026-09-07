<h1 align="center">
  <img src="../images/logo.svg" alt="AutoZWJ" width="180"><br>
  <strong>AutoZWJ</strong>
</h1>

<p align="center">
  AviUtl2 的插件<br>
  音MAD/YTPMV用对轨辅助插件，支持RPP/MIDI等多种工程格式。
</p>

<p align="center">
  <a href="https://github.com/TNOTawa/AutoZWJ/releases/latest">
    <img src="https://img.shields.io/github/v/release/TNOTawa/AutoZWJ?display_name=tag" alt="最新版本">
  </a>
  <a href="https://github.com/TNOTawa/AutoZWJ/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/TNOTawa/AutoZWJ" alt="许可证">
  </a>
  <a href="https://github.com/TNOTawa/AutoZWJ/releases/latest">
    <img src="https://img.shields.io/github/downloads/TNOTawa/AutoZWJ/total" alt="下载量">
  </a>
  <a href="https://github.com/TNOTawa/AutoZWJ">
    <img src="https://img.shields.io/github/stars/TNOTawa/AutoZWJ" alt="Stars">
  </a>
  <img src="https://img.shields.io/github/last-commit/TNOTawa/AutoZWJ" alt="最近提交">
</p>

<p align="center">
  <a href="../../README.md">English</a> |
  简体中文 |
  <a href="../ja/README.md">日本語</a>
</p>

将 REAPER 工程文件（`.rpp`）、标准 MIDI 文件（`.mid`）或 LRC 歌词文件（`.lrc`）中的素材，以用户在 AviUtl2 中选定的物件为模板，批量生成到时间轴上。

## 功能概述

- 解析 REAPER `.rpp`、标准 MIDI `.mid`、LRC `.lrc` 等工程文件
- 以时间轴上已有物件为样式模板，继承其效果链、参数设定
- 多源映射 —— 选中多个模板物件，按策略（顺序轮替 / 随机抽选 / 和弦映射）分配
- 效果链编辑器 —— 只读展示模板效果链，勾选参数 bake + 设置目标值 / 变量映射 / 表达式求值
- 脚本变量系统 —— `$note.velocity$ / 127 * 200` 等表达式驱动 bake 值
- BPM 网格同步 —— 将 MIDI/RPP 的 tempo map 写入 AviUtl2 的 BPM 网格
- 国际化支持 —— 界面支持简体中文 / English / 日本語，自动检测宿主 UI 语言
- 在模板下方自动分配图层，紧凑排列
- 交替翻转、物件时长控制、多音符策略、偶数项换行
- 所有参数自动从当前场景读取（FPS、分辨率），并跨会话持久化

## 安装

### 通过 AviUtl2 Catalog 安装

建议通过 [AviUtl2 Catalog](https://github.com/Neosku/aviutl2-catalog) 下载并安装 AutoZWJ，方便后续管理与更新。

### 直接拖拽安装

将 `AutoZWJ.aux2` 文件直接拖拽到 AviUtl2 的预览窗口，即可自动安装。

### 手动安装

将 `AutoZWJ.aux2` 放入 AviUtl2 的 `Plugin` 目录，启动 AviUtl2 即可加载。

## 快速上手

1. **开始使用**<br>
   在时间轴上**选中任意一个物件**，右键 → **配置导入...**。若时间轴上还没有物件，可先任意放置一个素材作为模板，或直接跳至第 2 步拖入工程文件

2. **导入工程**<br>
   若尚未导入工程，插件窗口会自动进入工程导入页：将 `.rpp` / `.mid` 文件拖入 AviUtl2 窗口，或在导入页选择 REAPER 最近文件 / 浏览...；随后勾选要生成的轨道，点击**确认导入**

3. **配置并生成**<br>
   在配置页调整参数 → 点击**确定**或**应用**

详细教程与功能说明请参阅 [文档](index.md)。

## 构建

需要 MinGW-w64 (g++ 15.2+)、CMake 3.20+。AviUtl2 SDK 以 git 子模块管理；Dear ImGui 已内联至 `src/thirdparty/imgui`（固定版本）。首次克隆后先初始化子模块：

```powershell
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles"
cmake --build build
```

产物为 `build/AutoZWJ.aux2`。正式发版由 GitHub Actions 在推送 `v*` tag 时自动构建 Release 并发布，详见 [docs/zh/build.md](build.md)。

## 参考

- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [RPPtoEXO ver2.0](https://github.com/Garech-mas/RPPtoEXO-ver2.0)
- [OtomadHelper](https://github.com/otomad/OtomadHelper)
- [om_midi](https://github.com/otomad/om_midi)
- [import_midi_tempos.aux2](https://github.com/sevenc-nanashi/import_midi_tempos.aux2)
- [UltraPaste](https://github.com/zzzzzz9125/UltraPaste)

## 参与贡献

欢迎参与 AutoZWJ 的开发与完善，包括提交代码修复、功能改进、文档更新和翻译。建议按照以下流程贡献：

1. Fork 本项目并创建独立分支
2. 完成修改，并在提交前进行必要的构建与测试
3. 提交 Pull Request，说明修改目的、主要变更和验证结果

涉及较大功能或架构调整时，建议先通过 Issue 讨论方案，再开始实现。

<p align="center">
  <a href="https://github.com/TNOTawa/AutoZWJ/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=tnotawa/autozwj" alt="贡献者">
  </a>
</p>

## 提交 Issue

遇到问题或有功能建议时，请在 [Issues](https://github.com/TNOTawa/AutoZWJ/issues) 中提交。提交问题时请尽量附带以下信息：

- AutoZWJ 版本、AviUtl2 版本和 Windows 版本
- 使用的工程格式（RPP、MIDI 或 LRC）及可复现的操作步骤
- 预期结果与实际结果
- 相关日志、截图，以及必要时经过脱敏的最小示例文件

提交前请先搜索已有 Issue，避免重复反馈。

## 捐赠

如果 AutoZWJ 对你有帮助，欢迎通过以下平台捐赠，支持项目持续开发。

<p align="center">
  <a href="https://ifdian.net/a/tnotawa">
    <img src="https://img.shields.io/badge/Afdian-Sponsor-946CE6?style=flat-square" alt="在爱发电赞助">
  </a>
  <a href="https://tnot.fanbox.cc/">
    <img src="https://img.shields.io/badge/pixivFANBOX-Sponsor-0096FA?style=flat-square" alt="在 pixivFANBOX 赞助">
  </a>
</p>

## 许可

MIT License
