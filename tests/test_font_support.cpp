// 简体中文界面字体判定测试（font_support）
// 判定规则不依赖具体机器；未安装的字体以 SKIP 输出，装有字体的用例逐项校验。
#include "ui/font_support.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>

static void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static std::string to_utf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        utf8.data(), size, nullptr, nullptr);
    return utf8;
}

static void test_text_detection() {
    check(text_has_simplified_only_glyphs(L"视频文件"), "视频文件 should be detected as Simplified Chinese");
    check(text_has_simplified_only_glyphs(L"音频文件"), "音频文件 should be detected as Simplified Chinese");
    check(text_has_simplified_only_glyphs(L"设置"), "设置 should be detected as Simplified Chinese");
    check(!text_has_simplified_only_glyphs(L"VideoFile"), "ASCII text is not Simplified Chinese");
    check(!text_has_simplified_only_glyphs(L"動画ファイル"), "Japanese text is not Simplified Chinese");
    check(!text_has_simplified_only_glyphs(L"視頻檔案"), "Traditional Chinese is not Simplified Chinese");
    check(!text_has_simplified_only_glyphs(L""), "empty text is not Simplified Chinese");
    std::printf("PASS: Simplified Chinese text detection\n");
}

static void test_uninstalled_font() {
    // 未安装的家族不能被 GDI 静默替换成系统中文字体
    check(!font_family_installed(L"__AutoZWJ missing font__"), "missing family should not be reported as installed");
    check(!font_covers_text(L"__AutoZWJ missing font__", kChineseUiProbeText), "missing family should not cover the probe text");
    check(!font_natively_supports_chinese_ui(L"__AutoZWJ missing font__"), "missing family is not native Chinese");
    check(!font_family_installed(L""), "empty family should not be installed");
    check(!font_natively_supports_chinese_ui(L""), "empty family is not native Chinese");
    std::printf("PASS: uninstalled font rejection\n");
}

static void test_installed_fonts() {
    struct Case {
        const wchar_t* family;
        bool native_chinese;
    };
    const Case cases[] = {
        { L"Microsoft YaHei", true },
        { L"Microsoft YaHei UI", true },
        { L"Noto Sans SC", true },
        { L"DengXian", true },
        { L"SimSun", true },
        { L"Yu Gothic UI", false },   // AviUtl2 默认主题字体：缺简体字形
        { L"Meiryo UI", false },
        { L"MS Gothic", false },
        { L"Segoe UI", false },
        { L"Microsoft JhengHei", false },  // 繁体字形
    };

    int evaluated = 0;
    for (const Case& c : cases) {
        if (!font_family_installed(c.family)) {
            std::printf("SKIP: font '%s' is not installed\n", to_utf8(c.family).c_str());
            continue;
        }
        evaluated++;
        const bool actual = font_natively_supports_chinese_ui(c.family);
        if (actual != c.native_chinese) {
            std::fprintf(stderr, "FAIL: font '%s' native Chinese verdict is %s, expected %s\n",
                         to_utf8(c.family).c_str(), actual ? "true" : "false",
                         c.native_chinese ? "true" : "false");
            std::exit(1);
        }
    }
    check(evaluated > 0, "at least one font case should be evaluated on this machine");
    std::printf("PASS: installed font verdicts (%d case(s) evaluated)\n", evaluated);
}

static void test_font_pick() {
    const std::wstring picked = pick_simplified_chinese_ui_font();
    const bool any_candidate_installed =
        font_family_installed(L"Microsoft YaHei") || font_family_installed(L"Microsoft YaHei UI") ||
        font_family_installed(L"Noto Sans SC") || font_family_installed(L"DengXian") ||
        font_family_installed(L"SimSun");
    if (!any_candidate_installed) {
        std::printf("SKIP: no Simplified Chinese candidate font is installed\n");
        return;
    }
    check(!picked.empty(), "a Chinese candidate font is installed, so a font should be picked");
    check(font_natively_supports_chinese_ui(picked), "picked font should natively support Chinese");
    check(font_covers_text(picked, kChineseUiProbeText), "picked font should cover the probe text");
    std::printf("PASS: picked Simplified Chinese UI font '%s'\n", to_utf8(picked).c_str());
}

int main() {
    test_text_detection();
    test_uninstalled_font();
    test_installed_fonts();
    test_font_pick();
    std::printf("PASS: font support tests passed\n");
    return 0;
}
