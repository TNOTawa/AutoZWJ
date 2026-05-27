#pragma once
#include "imgui.h"

extern ImFont* g_font_normal;
extern ImFont* g_font_bold;

namespace UI {
    // 字号
    constexpr float FONT_SIZE = 18.0f;

    // 行间距与内边距
    constexpr float ITEM_SPACING_Y = 8.0f;
    constexpr float FRAME_PADDING_Y = 6.0f;

    // 轨道树
    constexpr float TREE_INDENT = 24.0f;
    constexpr float TREE_LINE_HEIGHT = 28.0f;

    // 颜色
    constexpr ImVec4 COL_TRACK_NUM(1.0f, 0.85f, 0.4f, 1.0f);
    constexpr ImVec4 COL_TRACK_EMPTY(0.45f, 0.45f, 0.45f, 1.0f);
    constexpr ImVec4 COL_TREE_BRANCH(0.5f, 0.5f, 0.5f, 0.8f);
    constexpr ImVec4 COL_EFFECT_CHAIN_BG(0.28f, 0.28f, 0.42f, 1.0f);
    constexpr ImVec4 COL_EFFECT_CHAIN_HOVER(0.35f, 0.35f, 0.50f, 1.0f);
    constexpr ImVec4 COL_EFFECT_CHAIN_ACTIVE(0.22f, 0.22f, 0.35f, 1.0f);

    // 效果链编辑按钮宽度
    constexpr float EFFECT_CHAIN_BUTTON_WIDTH = 120.0f;
}
