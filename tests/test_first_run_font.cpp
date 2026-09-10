// 首次启动界面字体自动分配测试（端到端）
// 加载已构建的 AutoZWJ.aux2，用伪 CONFIG_HANDLE 模拟宿主语言与主题字体，
// 校验 preferences 写入结果：中文宿主自动分配简体中文字体，且只执行一次。
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "config2.h"
#include "ui/font_support.h"

using InitializeConfigFn = void (*)(CONFIG_HANDLE*);

namespace {

struct FakeHost {
    std::wstring app_data_path;
    bool simplified_chinese = true;
    std::wstring theme_font = L"Yu Gothic UI";
    FONT_INFO font_info{};
};

FakeHost g_fake;

LPCWSTR fake_language_text(CONFIG_HANDLE*, LPCWSTR, LPCWSTR text) {
    // 简体中文宿主返回语言文件中的译文；日文宿主原样返回键名（未翻译）
    if (!g_fake.simplified_chinese) return text;
    if (wcscmp(text, L"動画ファイル") == 0) return L"视频文件";
    if (wcscmp(text, L"音声ファイル") == 0) return L"音频文件";
    if (wcscmp(text, L"テキスト") == 0) return L"文本";
    return text;
}

FONT_INFO* fake_font_info(CONFIG_HANDLE*, LPCSTR) {
    g_fake.font_info.name = g_fake.theme_font.c_str();
    g_fake.font_info.size = 16.0f;
    return &g_fake.font_info;
}

void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

std::wstring read_ini_value(const std::filesystem::path& ini, const wchar_t* key) {
    wchar_t value[256] = {};
    GetPrivateProfileStringW(L"preferences", key, L"", value, 256, ini.c_str());
    return value;
}

void write_ini_value(const std::filesystem::path& ini, const wchar_t* key, const std::wstring& value) {
    WritePrivateProfileStringW(L"preferences", key, value.c_str(), ini.c_str());
}

std::filesystem::path make_case_dir(const wchar_t* name) {
    const auto dir = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    check(!ec, "case directory should be creatable");
    return dir;
}

// 用指定宿主状态调用一次 InitializeConfig，返回 preferences 文件路径
std::filesystem::path run_host(InitializeConfigFn initialize, const std::filesystem::path& dir,
                               bool simplified_chinese, const std::wstring& theme_font) {
    g_fake.app_data_path = dir.wstring();
    g_fake.simplified_chinese = simplified_chinese;
    g_fake.theme_font = theme_font;
    CONFIG_HANDLE config = {};
    config.app_data_path = g_fake.app_data_path.c_str();
    config.get_language_text = fake_language_text;
    config.get_font_info = fake_font_info;
    initialize(&config);
    return dir / L"AutoZWJ.preferences.ini";
}

}  // namespace

int main(int argc, char** argv) {
    check(argc > 1, "plugin DLL path argument is required");

    const int argc_w = MultiByteToWideChar(CP_ACP, 0, argv[1], -1, nullptr, 0);
    std::wstring dll_path(static_cast<size_t>(argc_w), L'\0');
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, dll_path.data(), argc_w);

    HMODULE dll = LoadLibraryW(dll_path.c_str());
    check(dll != nullptr, "AutoZWJ.aux2 should load");
    auto initialize = reinterpret_cast<InitializeConfigFn>(
        reinterpret_cast<void*>(GetProcAddress(dll, "InitializeConfig")));
    check(initialize != nullptr, "InitializeConfig export should exist");

    // 1) 简体中文宿主 + 未选字体（跟随主题 Yu Gothic UI）→ 自动分配简体中文字体
    {
        const auto dir = make_case_dir(L"autozwj_first_run_font_case1");
        const auto ini = run_host(initialize, dir, true, L"Yu Gothic UI");
        check(read_ini_value(ini, L"font_auto_setup_done") == L"1", "first run marker should be stored");
        check(read_ini_value(ini, L"font_name") == L"Microsoft YaHei",
              "Chinese host with a Japanese theme font should be assigned Microsoft YaHei");
        std::printf("PASS: Chinese host + Yu Gothic UI theme -> %ls\n",
                    read_ini_value(ini, L"font_name").c_str());
    }

    // 2) 简体中文宿主 + 已选择可用的简体中文字体 → 不覆盖用户选择
    //    字体安装情况随机器而异（CI 只装了兜底的微软雅黑），因此先探测本机可用的
    //    非兜底简体中文字体；探测不到时跳过该用例（与 autozwj_test_font 的 SKIP 约定一致）。
    {
        std::wstring already_usable;
        for (const wchar_t* candidate : { L"Microsoft YaHei UI", L"Noto Sans SC",
                                          L"DengXian", L"SimSun" }) {
            if (font_natively_supports_chinese_ui(candidate)) {
                already_usable = candidate;
                break;
            }
        }
        if (already_usable.empty()) {
            std::printf("SKIP: no non-fallback Simplified Chinese font is installed\n");
        } else {
            const auto dir = make_case_dir(L"autozwj_first_run_font_case2");
            write_ini_value(dir / L"AutoZWJ.preferences.ini", L"font_name", already_usable);
            const auto ini = run_host(initialize, dir, true, L"Yu Gothic UI");
            check(read_ini_value(ini, L"font_name") == already_usable,
                  "an already usable Chinese font should be kept");
            std::printf("PASS: Chinese host + %ls -> kept\n", already_usable.c_str());
        }
    }

    // 3) 非中文宿主 → 不干预字体
    {
        const auto dir = make_case_dir(L"autozwj_first_run_font_case3");
        const auto ini = run_host(initialize, dir, false, L"Yu Gothic UI");
        check(read_ini_value(ini, L"font_auto_setup_done") == L"1", "first run marker should be stored");
        check(read_ini_value(ini, L"font_name").empty(), "non-Chinese host should keep the theme font");
        std::printf("PASS: Japanese host -> font untouched\n");
    }

    // 4) 自动分配只执行一次：标记写入后即使用户改回缺字的主题字体也不再改写
    {
        const auto dir = make_case_dir(L"autozwj_first_run_font_case4");
        auto ini = run_host(initialize, dir, true, L"Yu Gothic UI");
        check(read_ini_value(ini, L"font_name") == L"Microsoft YaHei", "first run should assign a Chinese font");
        write_ini_value(ini, L"font_name", L"Yu Gothic UI");  // 用户改回主题字体
        ini = run_host(initialize, dir, true, L"Yu Gothic UI");
        check(read_ini_value(ini, L"font_name") == L"Yu Gothic UI",
              "the automatic assignment must not run twice");
        std::printf("PASS: automatic assignment runs only once\n");
    }

    // 5) 简体中文宿主 + 选择了不支持简体中文的字体 → 首次启动仍会分配简体中文字体
    {
        const auto dir = make_case_dir(L"autozwj_first_run_font_case5");
        write_ini_value(dir / L"AutoZWJ.preferences.ini", L"font_name", L"Consolas");
        const auto ini = run_host(initialize, dir, true, L"Yu Gothic UI");
        check(read_ini_value(ini, L"font_name") == L"Microsoft YaHei",
              "a font without Simplified Chinese glyphs should be replaced on first run");
        std::printf("PASS: Chinese host + Consolas -> Microsoft YaHei\n");
    }

    FreeLibrary(dll);
    std::printf("PASS: first run font tests passed\n");
    return 0;
}
