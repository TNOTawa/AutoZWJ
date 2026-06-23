#pragma once
#include "imgui.h"

extern ImFont* g_font_normal;
extern ImFont* g_font_bold;

namespace UI {
    // ---- 字号 ----
    constexpr float FONT_SIZE = 18.0f;

    // ---- 轨道树 ----
    constexpr float TREE_INDENT = 24.0f;
    constexpr float TREE_LINE_HEIGHT = 28.0f;

    // ---- 体系化圆角 ----
    constexpr float FRAME_ROUNDING = 4.0f;
    constexpr float CHILD_ROUNDING = 8.0f;
    constexpr float POPUP_ROUNDING = 6.0f;
    constexpr float SCROLLBAR_ROUNDING = 4.0f;
    constexpr float GRAB_ROUNDING = 3.0f;
    constexpr float TAB_ROUNDING = 6.0f;
    constexpr float WINDOW_ROUNDING = 0.0f;

    // ============================================================
    // 色彩系统（14+ 语义色）
    // ============================================================

    // 核心 6 基色
    constexpr ImVec4 COL_BG_DEEPEST(0.06f, 0.07f, 0.10f, 1.0f);
    constexpr ImVec4 COL_BG_SURFACE(0.10f, 0.11f, 0.15f, 1.0f);
    constexpr ImVec4 COL_BG_ELEVATED(0.14f, 0.15f, 0.20f, 1.0f);
    constexpr ImVec4 COL_BORDER_DIM(0.20f, 0.22f, 0.30f, 1.0f);
    constexpr ImVec4 COL_ACCENT(0.25f, 0.45f, 0.85f, 1.0f);
    constexpr ImVec4 COL_TEXT_PRIMARY(0.85f, 0.87f, 0.90f, 1.0f);

    // 核心衍生色
    constexpr ImVec4 COL_BG_DEEPEST_HOVER(0.08f, 0.09f, 0.13f, 1.0f);
    constexpr ImVec4 COL_BG_SURFACE_HOVER(0.20f, 0.21f, 0.28f, 1.0f);
    constexpr ImVec4 COL_BG_SURFACE_ACTIVE(0.08f, 0.09f, 0.12f, 1.0f);
    constexpr ImVec4 COL_POPUP_BG(COL_BG_ELEVATED.x, COL_BG_ELEVATED.y, COL_BG_ELEVATED.z, 0.90f);
    constexpr ImVec4 COL_ACCENT_HOVER(0.30f, 0.52f, 0.92f, 1.0f);
    constexpr ImVec4 COL_ACCENT_ACTIVE(0.20f, 0.38f, 0.75f, 1.0f);
    constexpr ImVec4 COL_TEXT_SECONDARY(0.55f, 0.57f, 0.59f, 1.0f);
    constexpr ImVec4 COL_TEXT_DISABLED(0.38f, 0.39f, 0.41f, 1.0f);
    constexpr ImVec4 COL_BG_FRAME(0.07f, 0.08f, 0.11f, 1.0f);
    constexpr ImVec4 COL_BG_FRAME_HOVER(0.12f, 0.13f, 0.18f, 1.0f);

    // 功能色
    constexpr ImVec4 COL_TRACK_NUM(1.0f, 0.85f, 0.4f, 1.0f);
    constexpr ImVec4 COL_TRACK_EMPTY(0.55f, 0.57f, 0.59f, 1.0f);
    constexpr ImVec4 COL_TREE_BRANCH(0.55f, 0.57f, 0.59f, 0.8f);
    constexpr ImVec4 COL_EFFECT_CHAIN_BG(0.28f, 0.28f, 0.42f, 1.0f);
    constexpr ImVec4 COL_EFFECT_CHAIN_HOVER(0.35f, 0.35f, 0.50f, 1.0f);
    constexpr ImVec4 COL_EFFECT_CHAIN_ACTIVE(0.22f, 0.22f, 0.35f, 1.0f);
    constexpr ImVec4 COL_WARNING_TEXT(0.9f, 0.8f, 0.3f, 1.0f);
    constexpr ImVec4 COL_HINT_TEXT(1.0f, 0.5f, 0.3f, 1.0f);
    constexpr ImVec4 COL_FLIP_FRAME(0.25f, 0.35f, 0.55f, 1.0f);
    constexpr ImVec4 COL_FLIP_FRAME_HOVER(0.30f, 0.40f, 0.60f, 1.0f);
    constexpr ImVec4 COL_FLIP_FRAME_ACTIVE(0.35f, 0.45f, 0.65f, 1.0f);
    constexpr ImVec4 COL_FLIP_HEADER(0.25f, 0.35f, 0.55f, 1.0f);
    constexpr ImVec4 COL_FLIP_HEADER_HOVER(0.30f, 0.40f, 0.60f, 1.0f);
    constexpr ImVec4 COL_FLIP_HEADER_ACTIVE(0.35f, 0.45f, 0.65f, 1.0f);
    constexpr ImU32 COL_DRAG_INSERT = IM_COL32(100, 200, 255, 255);

    // 效果链编辑按钮宽度
    constexpr float EFFECT_CHAIN_BUTTON_WIDTH = 120.0f;

    // ---- 主题注入 ----
    inline void ApplyTheme(ImGuiStyle& style) {
        style.FrameRounding    = FRAME_ROUNDING;
        style.ChildRounding     = CHILD_ROUNDING;
        style.PopupRounding     = POPUP_ROUNDING;
        style.ScrollbarRounding = SCROLLBAR_ROUNDING;
        style.GrabRounding      = GRAB_ROUNDING;
        style.TabRounding       = TAB_ROUNDING;
        style.WindowRounding    = WINDOW_ROUNDING;

        style.FrameBorderSize   = 0.0f;
        style.ChildBorderSize   = 1.0f;
        style.PopupBorderSize   = 1.0f;

        style.ItemSpacing       = ImVec2(8, 8);
        style.FramePadding      = ImVec2(8, 6);
        style.WindowPadding     = ImVec2(16, 14);
        style.CellPadding       = ImVec2(8, 6);

        ImVec4* c = style.Colors;

        c[ImGuiCol_Text]                  = COL_TEXT_PRIMARY;
        c[ImGuiCol_TextDisabled]          = COL_TEXT_DISABLED;
        c[ImGuiCol_WindowBg]              = COL_BG_DEEPEST;
        c[ImGuiCol_ChildBg]               = COL_BG_SURFACE;
        c[ImGuiCol_PopupBg]               = COL_POPUP_BG;
        c[ImGuiCol_Border]                = COL_BORDER_DIM;
        c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg]               = COL_BG_FRAME;
        c[ImGuiCol_FrameBgHovered]        = COL_BG_FRAME_HOVER;
        c[ImGuiCol_FrameBgActive]         = COL_BG_SURFACE_ACTIVE;
        c[ImGuiCol_TitleBg]               = COL_BG_DEEPEST;
        c[ImGuiCol_TitleBgActive]         = COL_BG_ELEVATED;
        c[ImGuiCol_TitleBgCollapsed]      = COL_BG_DEEPEST;
        c[ImGuiCol_MenuBarBg]             = COL_BG_ELEVATED;
        c[ImGuiCol_ScrollbarBg]           = COL_BG_SURFACE;
        c[ImGuiCol_ScrollbarGrab]         = COL_BG_ELEVATED;
        c[ImGuiCol_ScrollbarGrabHovered]  = COL_BG_SURFACE_HOVER;
        c[ImGuiCol_ScrollbarGrabActive]   = COL_ACCENT_ACTIVE;
        c[ImGuiCol_CheckMark]             = COL_ACCENT;
        c[ImGuiCol_SliderGrab]            = COL_ACCENT;
        c[ImGuiCol_SliderGrabActive]      = COL_ACCENT_ACTIVE;
        c[ImGuiCol_Button]                = COL_ACCENT;
        c[ImGuiCol_ButtonHovered]         = COL_ACCENT_HOVER;
        c[ImGuiCol_ButtonActive]          = COL_ACCENT_ACTIVE;
        c[ImGuiCol_Header]                = COL_BG_ELEVATED;
        c[ImGuiCol_HeaderHovered]         = COL_BG_SURFACE_HOVER;
        c[ImGuiCol_HeaderActive]          = COL_BG_SURFACE_ACTIVE;
        c[ImGuiCol_Separator]             = COL_BORDER_DIM;
        c[ImGuiCol_SeparatorHovered]      = COL_TEXT_SECONDARY;
        c[ImGuiCol_SeparatorActive]       = COL_ACCENT;
        c[ImGuiCol_ResizeGrip]            = ImVec4(0.25f, 0.45f, 0.85f, 0.4f);
        c[ImGuiCol_ResizeGripHovered]     = COL_ACCENT_HOVER;
        c[ImGuiCol_ResizeGripActive]      = COL_ACCENT_ACTIVE;
        c[ImGuiCol_Tab]                   = COL_BG_SURFACE;
        c[ImGuiCol_TabHovered]            = COL_BG_SURFACE_HOVER;
        c[ImGuiCol_TabActive]             = COL_ACCENT;
        c[ImGuiCol_TabUnfocused]          = COL_BG_SURFACE;
        c[ImGuiCol_TabUnfocusedActive]    = COL_BG_ELEVATED;
        c[ImGuiCol_DockingPreview]        = ImVec4(0.25f, 0.45f, 0.85f, 0.4f);
        c[ImGuiCol_DockingEmptyBg]        = COL_BG_DEEPEST;
        c[ImGuiCol_PlotLines]             = COL_ACCENT;
        c[ImGuiCol_PlotLinesHovered]      = COL_ACCENT_HOVER;
        c[ImGuiCol_PlotHistogram]         = COL_ACCENT;
        c[ImGuiCol_PlotHistogramHovered]  = COL_ACCENT_HOVER;
        c[ImGuiCol_TableHeaderBg]         = COL_BG_ELEVATED;
        c[ImGuiCol_TableBorderStrong]     = COL_BORDER_DIM;
        c[ImGuiCol_TableBorderLight]      = ImVec4(0.14f, 0.15f, 0.21f, 1.0f);
        c[ImGuiCol_TableRowBg]            = COL_BG_SURFACE;
        c[ImGuiCol_TableRowBgAlt]         = COL_BG_SURFACE_HOVER;
        c[ImGuiCol_TextSelectedBg]        = COL_ACCENT;
        c[ImGuiCol_DragDropTarget]        = COL_ACCENT;
        c[ImGuiCol_NavHighlight]          = COL_ACCENT;
        c[ImGuiCol_NavWindowingHighlight] = COL_ACCENT;
        c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.06f, 0.07f, 0.10f, 0.6f);
        c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.06f, 0.07f, 0.10f, 0.6f);
    }
}
