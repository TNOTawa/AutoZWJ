[中文](../../README.md) | [English](.) | [日本語](../ja/README.md)

> **Note:** i18n support for languages other than Chinese is not yet available. Please wait for the next version.

# AutoZWJ

> RPP / MIDI → AviUtl2 Object Batch Import Plugin

Import media items from REAPER project files (`.rpp`) or standard MIDI files (`.mid`) into the AviUtl2 timeline as objects, using existing objects as style templates.

## Features

- Parse REAPER `.rpp` / standard MIDI `.mid` project files (more formats coming soon)
- Use existing timeline objects as **style templates**, inheriting their effect chains and parameters
- **Multi-source mapping** — assign templates using configurable strategies (sequential rotation / random selection / chord mapping)
- **Effect chain editor** — view template effect chains, bake parameters with fixed values / variable mapping / expression evaluation
- **Script variable system** — expression-driven bake values like `$note.velocity$ / 127 * 200`
- Automatic layer allocation below the template, compactly arranged
- Alternating flips, object duration control, multi-note strategies, alternating row layout
- All parameters automatically read from the current scene (FPS, resolution) and persisted across sessions

## Installation

Place `AutoZWJ.aux2` in AviUtl2's `Plugin` directory, then launch AviUtl2.

## Quick Start

1. **Load a project**  
   Right-click empty area on timeline → **Select audio project...** → choose a `.rpp` or `.mid` file

2. **Select templates**  
   Select one or more objects on the timeline to use as style templates (effect chains, filters, etc. will be inherited)

3. **Configure and generate**  
   Right-click selected objects → **Configure import...** → adjust parameters in the popup window → click **OK** or **Apply**

For detailed tutorials and feature explanations, see the [documentation](../zh/).

## Build

Requires MinGW-w64 (g++ 15.2+), CMake 3.20+, Dear ImGui docking branch.

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build
```

Output: `build/AutoZWJ.aux2`.

See [Build Guide](../zh/build.md) for details.

## References

- [RPPtoEXO ver2.0](https://github.com/Garech-mas/RPPtoEXO-ver2.0)
- [OtomadHelper](https://github.com/otomad/OtomadHelper)
- [om_midi](https://github.com/otomad/om_midi)
- [AviUtl2 / AviUtl ExEdit2 Plugin SDK](https://spring-fragrance.mints.ne.jp/aviutl/)
- [Dear ImGui](https://github.com/ocornut/imgui)

## License

MIT License
