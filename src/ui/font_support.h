#pragma once
#include <string>

// 简体中文界面探针文本：取自插件界面的高频词，其中「导/设/视/频/项/预/帧/链/轨/置」
// 等字形仅存在于简体字体中，用于判断字体是否会把中文界面显示成问号或日文字形。
extern const wchar_t* const kChineseUiProbeText;

// 文本是否包含简体专用字形（繁体/日文使用不同字形），用于识别宿主界面语言
bool text_has_simplified_only_glyphs(const std::wstring& text);

// 字体家族是否真实存在（通过字体枚举确认，未安装的家族不会被 GDI 静默替换）
bool font_family_installed(const std::wstring& family);

// 字体是否覆盖给定文本的全部字符（缺字在界面上会显示为问号）
bool font_covers_text(const std::wstring& family, const std::wstring& text);

// 字体是否原生支持简体中文界面：
//   1) 家族真实存在（通过字体枚举确认，避免 GDI 静默替换）
//   2) 探针文本全部有字形
//   3) 字体签名声明 GB2312 代码页，即简体中文，而非只声明日文代码页
//      （Yu Gothic UI / Meiryo / MS Gothic 等日文字体在此被排除）
bool font_natively_supports_chinese_ui(const std::wstring& family);

// 按优先级选择已安装且原生支持简体中文的界面字体（微软雅黑等），失败返回空串
std::wstring pick_simplified_chinese_ui_font();
