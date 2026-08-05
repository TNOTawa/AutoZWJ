#include "tools/up/up_media_parser.h"
#include "codec/codec.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

static void check(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

static bool read_file_bytes(const char* path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    f.seekg(0, std::ios::beg);
    if (len < 0) return false;
    out.resize((size_t)len);
    if (len > 0) f.read((char*)out.data(), len);
    return f.good() || len == 0;
}

// 对每个 fixture 断言结构不变量
static void check_project(const UpProject& proj, const char* path) {
    check(!proj.tracks.empty(), "at least one track expected");
    for (const auto& track : proj.tracks) {
        check(!track.items.empty(), "each parsed track should contain items");
        for (const auto& item : track.items) {
            check(!item.file_path.empty(), "item file path should not be empty");
            check(is_valid_utf8(item.file_path), "item file path should be valid UTF-8 after conversion");
            check(item.position >= 0.0, "item position should be >= 0");
            check(item.length > 0.0, "item length should be > 0");
            check(item.playrate != 0.0, "item playrate should not be zero");
            check(item.source_type == "VIDEO" || item.source_type == "AUDIO",
                  "item source type should be VIDEO or AUDIO");
        }
    }
    std::printf("  fixture ok: %s (%zu tracks)\n", path, proj.tracks.size());
}

static bool close_to(double a, double b, double eps = 1e-9) {
    return (a > b - eps) && (a < b + eps);
}

// basic.txt：真实 REAPER 剪贴板 dump（81 个根级 ITEM，无 TRACK 块，UTF-8 中文路径）
// 注：原始 dump 中的个人文件路径已脱敏为通用路径，其余内容未改动
static void check_basic_fixture(const UpProject& proj) {
    check(proj.tracks.size() == 1, "basic.txt should parse as a single track");
    check(proj.tracks[0].items.size() == 81, "basic.txt should contain 81 items");

    const auto& it0 = proj.tracks[0].items[0];
    check(close_to(it0.position, 7.37766041601422), "item[0] position mismatch");
    check(close_to(it0.length, 0.00721520199686), "item[0] length mismatch");
    check(it0.loop, "item[0] loop should be true");
    check(close_to(it0.soffs, 0.06166019794433), "item[0] soffs mismatch");
    check(close_to(it0.playrate, 1.0), "item[0] playrate mismatch");
    check(it0.source_type == "VIDEO", "item[0] source type should be VIDEO");
    check(it0.file_path == u8"C:\\测试媒体\\样例.mp4",
          "item[0] file path should be the sanitized generic path");

    const auto& it1 = proj.tracks[0].items[1];
    check(close_to(it1.playrate, 0.49723724856708), "item[1] playrate should come from the first PLAYRATE token");

    const auto& last = proj.tracks[0].items.back();
    check(close_to(last.position, 21.441283498362), "last item position mismatch");
    check(last.file_path == u8"C:\\测试媒体\\样例.mp4", "last item file path mismatch");

    std::printf("  basic.txt exact-value assertions ok\n");
}

int main() {
    std::printf("== up parser tests ==\n");

    // 无意义数据必须解析失败
    {
        std::vector<uint8_t> garbage = {'h', 'e', 'l', 'l', 'o'};
        UpProject proj;
        check(!parse_reaper_media(garbage, proj), "garbage data should fail to parse");
        std::printf("  garbage rejected ok\n");
    }

    // 真实 REAPERMedia 剪贴板 dump（在 REAPER 中复制媒体后用 dump_clipboard 采集，
    // 存放于 tests/fixtures/up/ 下，运行时工作目录为仓库根目录）
    const char* fixtures[] = {
        "tests/fixtures/up/basic.txt",
        "tests/fixtures/up/multi_track.txt",
        "tests/fixtures/up/section.txt",
    };

    int loaded = 0;
    bool basic_checked = false;
    for (const char* path : fixtures) {
        std::vector<uint8_t> data;
        if (!read_file_bytes(path, data)) continue;
        loaded++;

        UpProject proj;
        check(parse_reaper_media(data, proj), "parse_reaper_media should succeed");
        check_project(proj, path);
        if (std::string(path) == "tests/fixtures/up/basic.txt") {
            check_basic_fixture(proj);
            basic_checked = true;
        }
    }
    check(basic_checked, "basic.txt fixture should be present");

    if (loaded == 0) {
        std::printf("SKIP: no fixture in tests/fixtures/up/ (capture one via dump_clipboard, run from repo root)\n");
        return 0;
    }

    std::printf("PASS: all up parser tests passed (%d fixtures)\n", loaded);
    return 0;
}
