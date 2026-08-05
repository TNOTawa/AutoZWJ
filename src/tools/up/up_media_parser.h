#pragma once
#include <cstdint>
#include <string>
#include <vector>

// REAPERMedia 剪贴板格式的媒体项
struct UpItem {
    double position = 0.0;
    double length = 0.0;
    double playrate = 1.0;
    double soffs = 0.0;
    bool loop = false;
    std::string file_path;
    std::string source_type;   // VIDEO / AUDIO
};

struct UpTrack {
    std::wstring name;
    std::vector<UpItem> items;
};

struct UpProject {
    std::vector<UpTrack> tracks;
};

// 解析 REAPER 剪贴板（REAPERMedia 格式）数据，数据为 NUL/CRLF 分隔的 chunk 文本。
// 解析成功（存在至少一条含媒体项的轨道）返回 true。
bool parse_reaper_media(const std::vector<uint8_t>& data, UpProject& proj);
