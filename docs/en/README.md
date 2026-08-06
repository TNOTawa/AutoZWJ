[中文](../../README.md) | [English](.) | [日本語](../ja/README.md)

# AutoZWJ

> RPP / MIDI → AviUtl2 Object Batch Import Plugin

Import media items from REAPER project files (`.rpp`), standard MIDI files (`.mid`), or LRC lyrics files (`.lrc`) into the AviUtl2 timeline as objects, using existing objects as style templates.

## Features

- Parse REAPER `.rpp` / standard MIDI `.mid` / LRC `.lrc` files
- Use existing timeline objects as **style templates**, inheriting their effect chains and parameters
- **Multi-source mapping** — assign templates using configurable strategies (sequential rotation / random selection / chord mapping)
- **Effect chain editor** — view template effect chains, bake parameters with fixed values / variable mapping / expression evaluation
- **Script variable system** — expression-driven bake values like `$note.velocity$ / 127 * 200`
- **BPM grid sync** — apply tempo maps from MIDI/RPP to AviUtl2's BPM grid
- **i18n support** — UI available in Chinese, English, Japanese; auto-detects host UI language
- Automatic layer allocation below the template, compactly arranged
- Alternating flips, object duration control, multi-note strategies, alternating row layout
- All parameters automatically read from the current scene (FPS, resolution) and persisted across sessions

## Installation

Place `AutoZWJ.aux2` in AviUtl2's `Plugin` directory, then launch AviUtl2.

## Quick Start

1. **Start**  
   Select **any object** on the timeline, right-click → **Configure import...**. If the timeline is empty, place any object first to serve as a template, or skip to step 2 and drop a project file

2. **Load a project**  
   If no project is loaded yet, the plugin window opens on the import page automatically: drop a `.rpp` / `.mid` file onto the AviUtl2 window, or choose a REAPER recent file / **Browse...** on the import page; then check the tracks to generate and click **Confirm import**

3. **Configure and generate**  
   Adjust parameters on the config page → click **OK** or **Apply**

For detailed tutorials and feature explanations, see the [documentation](../zh/).

## Build

Requires MinGW-w64 (g++ 15.2+), CMake 3.20+. The AviUtl2 SDK and Dear ImGui are managed as git submodules; initialize them after the first clone:

```powershell
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G "MinGW Makefiles"
cmake --build build
```

Output: `build/AutoZWJ.aux2`.

Official releases are built by GitHub Actions on `v*` tag pushes and published as GitHub Releases. See [Build Guide](../zh/build.md) for details.

## References

- [RPPtoEXO ver2.0](https://github.com/Garech-mas/RPPtoEXO-ver2.0)
- [OtomadHelper](https://github.com/otomad/OtomadHelper)
- [om_midi](https://github.com/otomad/om_midi)
- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)

## License

MIT License
