#include "ui/font_support.h"
#include <windows.h>
#include <string>
#include <string_view>
#include <vector>

const wchar_t* const kChineseUiProbeText =
    L"导入配置轨道物件图层帧时长翻转间隙模板效果链字体设置视音频工程件项预览";

namespace {

// 仅存在于简体字形的界面常用字（繁体/日文使用不同字形）
constexpr std::wstring_view kSimplifiedOnlyChars =
    L"视频图设项帧导轨层链轴轮绝续录忆无时门发这边电车华体说语请读记认计选调节约级给"
    L"为仅优则创删动单历变吗场宽对并应开复换据数机条标检浏独确紧组终结编网览许试该负败贴转载辑过进连针键长闭间随页顺预题齐号户";

// GDI FONTSIGNATURE 代码页位：位 18 为 936（GB2312，简体中文），位 17 为 932（日文）
constexpr DWORD kGb2312CodePageBit = 0x00040000;

struct FamilyEnumContext {
    std::wstring resolved;
};

int CALLBACK enum_family_proc(const LOGFONTW* logfont, const TEXTMETRICW*, DWORD, LPARAM param) {
    auto* ctx = reinterpret_cast<FamilyEnumContext*>(param);
    if (ctx->resolved.empty() && logfont->lfFaceName[0]) ctx->resolved = logfont->lfFaceName;
    return 0;  // 只需要第一个匹配项
}

// 通过字体枚举确认家族真实存在，并取得 GDI 实际使用的家族名（可能是本地化名称）。
// 直接 CreateFontW 时未安装的家族会被静默替换成系统默认字体，枚举可避免这一误判。
bool resolve_family(HDC dc, const std::wstring& family, std::wstring& resolved) {
    if (family.empty()) return false;
    LOGFONTW logfont = {};
    logfont.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(logfont.lfFaceName, family.c_str(), LF_FACESIZE);
    FamilyEnumContext ctx;
    EnumFontFamiliesExW(dc, &logfont, enum_family_proc, reinterpret_cast<LPARAM>(&ctx), 0);
    if (ctx.resolved.empty()) return false;
    resolved = ctx.resolved;
    return true;
}

// 选定字体并读取字形/签名信息，析构时恢复 DC 状态
struct FontProbe {
    HDC dc = nullptr;
    HFONT font = nullptr;
    HGDIOBJ previous = nullptr;
    FONTSIGNATURE signature = {};

    ~FontProbe() {
        if (!dc) return;
        if (font) {
            SelectObject(dc, previous);
            DeleteObject(font);
        }
        ReleaseDC(nullptr, dc);
    }

    bool open(const std::wstring& family) {
        if (family.empty()) return false;
        dc = GetDC(nullptr);
        if (!dc) return false;
        std::wstring resolved;
        if (!resolve_family(dc, family, resolved)) return false;
        font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH,
            resolved.c_str());
        if (!font) return false;
        previous = SelectObject(dc, font);
        GetTextCharsetInfo(dc, &signature, 0);
        return true;
    }

    bool covers(const std::wstring& text) const {
        if (!dc || text.empty()) return false;
        std::vector<WORD> glyphs(text.size());
        if (!GetGlyphIndicesW(dc, text.c_str(), static_cast<int>(text.size()), glyphs.data(),
                              GGI_MARK_NONEXISTING_GLYPHS))
            return false;
        for (WORD glyph : glyphs) {
            if (glyph == 0xFFFF) return false;
        }
        return true;
    }
};

}  // namespace

bool text_has_simplified_only_glyphs(const std::wstring& text) {
    for (wchar_t c : text) {
        if (kSimplifiedOnlyChars.find(c) != std::wstring_view::npos) return true;
    }
    return false;
}

bool font_family_installed(const std::wstring& family) {
    HDC dc = GetDC(nullptr);
    if (!dc) return false;
    std::wstring resolved;
    const bool installed = resolve_family(dc, family, resolved);
    ReleaseDC(nullptr, dc);
    return installed;
}

bool font_covers_text(const std::wstring& family, const std::wstring& text) {
    FontProbe probe;
    return probe.open(family) && probe.covers(text);
}

bool font_natively_supports_chinese_ui(const std::wstring& family) {
    FontProbe probe;
    if (!probe.open(family)) return false;
    if (!probe.covers(kChineseUiProbeText)) return false;
    return (probe.signature.fsCsb[0] & kGb2312CodePageBit) != 0;
}

std::wstring pick_simplified_chinese_ui_font() {
    static const wchar_t* const kCandidates[] = {
        L"Microsoft YaHei",
        L"Microsoft YaHei UI",
        L"Noto Sans SC",
        L"DengXian",
        L"SimSun",
    };
    for (const wchar_t* candidate : kCandidates) {
        if (font_natively_supports_chinese_ui(candidate)) return candidate;
    }
    return std::wstring();
}
