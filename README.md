<h1 align="center">
  <img src="docs/images/logo.svg" alt="AutoZWJ" width="180"><br>
  <strong>AutoZWJ</strong>
</h1>

<p align="center">
  A plugin for AviUtl2<br>
  An Otomad/YTPMV track alignment assistant supporting RPP, MIDI, and other project formats.
</p>

<p align="center">
  <a href="https://github.com/TNOTawa/AutoZWJ/releases/latest">
    <img src="https://img.shields.io/github/v/release/TNOTawa/AutoZWJ?display_name=tag" alt="Latest release">
  </a>
  <a href="https://github.com/TNOTawa/AutoZWJ/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/TNOTawa/AutoZWJ" alt="License">
  </a>
  <a href="https://github.com/TNOTawa/AutoZWJ/releases/latest">
    <img src="https://img.shields.io/github/downloads/TNOTawa/AutoZWJ/total" alt="Downloads">
  </a>
  <a href="https://github.com/TNOTawa/AutoZWJ">
    <img src="https://img.shields.io/github/stars/TNOTawa/AutoZWJ" alt="Stars">
  </a>
  <img src="https://img.shields.io/github/last-commit/TNOTawa/AutoZWJ" alt="Last commit">
</p>

<p align="center">
  English |
  <a href="docs/zh/README.md">简体中文</a> |
  <a href="docs/ja/README.md">日本語</a>
</p>

Import media items from REAPER project files (`.rpp`), standard MIDI files (`.mid`), or LRC lyrics files (`.lrc`) into the AviUtl2 timeline as objects, using existing objects as style templates.

## Features

- Parse REAPER `.rpp` / standard MIDI `.mid` / LRC `.lrc` files
- Use existing timeline objects as style templates, inheriting their effect chains and parameters
- Multi-source mapping: assign multiple templates using configurable strategies (sequential rotation / random selection / chord mapping / animation sequence)
- Effect chain editor: view template effect chains and bake parameters with fixed values / variable mappings / evaluated expressions
- Script variable system: drive bake values with expressions such as `$note.velocity$ / 127 * 200`
- BPM grid sync: apply MIDI/RPP tempo maps to AviUtl2's BPM grid
- Internationalization: UI available in Simplified Chinese, English, and Japanese, with automatic host UI language detection
- Automatic layer allocation below the template, compactly arranged
- Alternating flips, object duration control, multi-note strategies, alternating row layout
- All parameters automatically read from the current scene (FPS, resolution) and persisted across sessions

## Installation

### Install with AviUtl2 Catalog

We recommend installing AutoZWJ through [AviUtl2 Catalog](https://github.com/Neosku/aviutl2-catalog) for easier management and updates.

### Drag-and-drop installation

Drag the `AutoZWJ.aux2` file directly into AviUtl2's preview window to install it automatically.

### Manual installation

Place `AutoZWJ.aux2` in AviUtl2's `Plugin` directory, then launch AviUtl2.

## Quick Start

1. **Start**<br>
   Select **any object** on the timeline, right-click → **Configure import...**. If the timeline is empty, place any object first to serve as a template, or skip to step 2 and drop a project file

2. **Load a project**<br>
   If no project is loaded yet, the plugin window opens on the import page automatically: drop a `.rpp` / `.mid` file onto the AviUtl2 window, or choose a REAPER recent file / **Browse...** on the import page; then check the tracks to generate and click **Confirm import**

3. **Configure and generate**<br>
   Adjust parameters on the config page → click **OK** or **Apply**

For detailed tutorials and feature explanations, see the [documentation](docs/en/index.md).

## Build

Requires MinGW-w64 (g++ 15.2+), CMake 3.20+. The AviUtl2 SDK is managed as a git submodule; Dear ImGui is vendored under `src/thirdparty/imgui` at a fixed version. After the first clone, initialize the submodule:

```powershell
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles"
cmake --build build
```

Output: `build/AutoZWJ.aux2`.

Official releases are built by GitHub Actions on `v*` tag pushes and published as GitHub Releases. See the [build guide](docs/zh/build.md) for details.

## References

- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [RPPtoEXO ver2.0](https://github.com/Garech-mas/RPPtoEXO-ver2.0)
- [OtomadHelper](https://github.com/otomad/OtomadHelper)
- [om_midi](https://github.com/otomad/om_midi)
- [import_midi_tempos.aux2](https://github.com/sevenc-nanashi/import_midi_tempos.aux2)
- [UltraPaste](https://github.com/zzzzzz9125/UltraPaste)

## Contributing

Contributions to AutoZWJ are welcome, including bug fixes, feature improvements, documentation updates, and translations. We recommend the following process:

1. Fork this repository and create a dedicated branch
2. Make your changes and perform the necessary builds and tests before committing
3. Open a Pull Request describing the purpose, key changes, and verification results

For major features or architectural changes, please discuss the proposal in an Issue before starting implementation.

<p align="center">
  <a href="https://github.com/TNOTawa/AutoZWJ/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=tnotawa/autozwj" alt="Contributors">
  </a>
</p>

## Reporting Issues

If you encounter a problem or have a feature request, submit it through [Issues](https://github.com/TNOTawa/AutoZWJ/issues). Please include as much of the following information as possible:

- AutoZWJ version, AviUtl2 version, and Windows version
- Project format used (RPP, MIDI, or LRC) and reproducible steps
- Expected result and actual result
- Relevant logs, screenshots, and a sanitized minimal sample file when necessary

Please search existing Issues before submitting to avoid duplicates.

## Donate

If AutoZWJ is useful to you, you can support its continued development through either platform below.

<p align="center">
  <a href="https://ifdian.net/a/tnotawa">
    <img src="https://img.shields.io/badge/Afdian-Sponsor-946CE6?style=flat-square" alt="Support on Afdian">
  </a>
  <a href="https://tnot.fanbox.cc/">
    <img src="https://img.shields.io/badge/pixivFANBOX-Sponsor-0096FA?style=flat-square" alt="Support on pixivFANBOX">
  </a>
</p>

## License

MIT License
