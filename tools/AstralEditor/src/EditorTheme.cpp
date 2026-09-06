#include "Astral/Editor/EditorTheme.hpp"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Astral {

void ApplyAstralTheme(bool isPlayMode) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // ── Geometry (Photoshop-style sharp panels) ─────────────────
    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 2.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 2.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    style.WindowPadding     = ImVec2(12.0f, 10.0f);
    style.FramePadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 8.0f;

    style.WindowTitleAlign   = ImVec2(0.0f, 0.5f);
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);

    style.DockingSeparatorSize = 4.0f;
    style.TabBarBorderSize = 1.0f;
    style.TabBarOverlineSize = 2.0f;
    style.ChildBorderSize = 1.0f;
    style.CellPadding = ImVec2(6.0f, 5.0f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.WindowMinSize = ImVec2(180.0f, 120.0f);

    using namespace EditorPalette;
    const ImVec4 bg_canvas = Canvas, bg_panel = Panel, bg_popup = Raised;
    const ImVec4 bg_header = Canvas, bg_menubar = Canvas, bg_input = Input;
    const ImVec4 bg_hover = Hover, bg_active = Selection;
    const ImVec4 bg_tab_active = Raised, bg_tab_inactive = Canvas;
    const ImVec4 accent = Accent, accent_hover = AccentHover, accent_active = Accent;
    const ImVec4 border = Border, border_shadow(0, 0, 0, 0);
    const ImVec4 text_primary = Text, text_disabled = Muted;
    const ImVec4 scrollbar_grab = Border, scrollbar_hover = Muted, scrollbar_active = AccentHover;
    // ── Apply Colors ────────────────────────────────────────────
    colors[ImGuiCol_Text]                   = text_primary;
    colors[ImGuiCol_TextDisabled]           = text_disabled;
    colors[ImGuiCol_WindowBg]               = bg_panel;
    colors[ImGuiCol_ChildBg]                = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_PopupBg]                = bg_popup;
    colors[ImGuiCol_Border]                 = border;
    colors[ImGuiCol_BorderShadow]           = border_shadow;

    colors[ImGuiCol_FrameBg]                = bg_input;
    colors[ImGuiCol_FrameBgHovered]         = bg_hover;
    colors[ImGuiCol_FrameBgActive]          = bg_active;

    colors[ImGuiCol_TitleBg]                = bg_header;
    colors[ImGuiCol_TitleBgActive]          = bg_header;
    colors[ImGuiCol_TitleBgCollapsed]       = bg_header;

    colors[ImGuiCol_MenuBarBg]              = bg_menubar;

    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ScrollbarGrab]          = scrollbar_grab;
    colors[ImGuiCol_ScrollbarGrabHovered]   = scrollbar_hover;
    colors[ImGuiCol_ScrollbarGrabActive]    = scrollbar_active;

    colors[ImGuiCol_CheckMark]              = accent;
    colors[ImGuiCol_SliderGrab]             = accent;
    colors[ImGuiCol_SliderGrabActive]       = accent_hover;

    colors[ImGuiCol_Button]                 = Raised;
    colors[ImGuiCol_ButtonHovered]          = bg_hover;
    colors[ImGuiCol_ButtonActive]           = accent_active;

    colors[ImGuiCol_Header]                 = Selection;
    colors[ImGuiCol_HeaderHovered]          = bg_hover;
    colors[ImGuiCol_HeaderActive]           = accent_active;

    colors[ImGuiCol_Separator]              = Border;
    colors[ImGuiCol_SeparatorHovered]       = accent_hover;
    colors[ImGuiCol_SeparatorActive]        = accent_active;

    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ResizeGripHovered]      = bg_hover;
    colors[ImGuiCol_ResizeGripActive]       = accent_active;

    colors[ImGuiCol_Tab]                    = bg_tab_inactive;
    colors[ImGuiCol_TabHovered]             = bg_hover;
    colors[ImGuiCol_TabSelected]              = bg_tab_active;
    colors[ImGuiCol_TabDimmed]           = bg_tab_inactive;
    colors[ImGuiCol_TabDimmedSelected]     = bg_tab_active;

    colors[ImGuiCol_TabSelectedOverline]    = accent_hover;
    colors[ImGuiCol_TabDimmedSelectedOverline] = accent;
    colors[ImGuiCol_DockingPreview]         = ImVec4(accent.x, accent.y, accent.z, 0.65f);
    colors[ImGuiCol_DockingEmptyBg]         = bg_canvas;

    colors[ImGuiCol_PlotLines]              = accent;
    colors[ImGuiCol_PlotLinesHovered]       = accent_hover;
    colors[ImGuiCol_PlotHistogram]          = accent;
    colors[ImGuiCol_PlotHistogramHovered]   = accent_hover;

    colors[ImGuiCol_TableHeaderBg]          = bg_header;
    colors[ImGuiCol_TableBorderStrong]      = border;
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.196f, 0.196f, 0.196f, 1.0f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.0f, 0.0f, 0.0f, 0.04f);

    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.176f, 0.365f, 0.667f, 0.40f);
    colors[ImGuiCol_DragDropTarget]         = accent_hover;
    colors[ImGuiCol_NavHighlight]           = accent;
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);

    // Reapply the base palette first so Stop restores every authoring color.
    // Docked windows display tabs in place of title bars: tint both surfaces.
    if (isPlayMode) {
        const ImVec4 runtimeTitle(0.125f, 0.153f, 0.125f, 1.0f);
        const ImVec4 runtimeBorder(0.310f, 0.365f, 0.247f, 1.0f);
        colors[ImGuiCol_TitleBg] = runtimeTitle;
        colors[ImGuiCol_TitleBgActive] = runtimeTitle;
        colors[ImGuiCol_TitleBgCollapsed] = runtimeTitle;
        colors[ImGuiCol_Tab] = runtimeTitle;
        colors[ImGuiCol_TabDimmed] = runtimeTitle;
        colors[ImGuiCol_TabSelected] = ImVec4(0.173f, 0.208f, 0.161f, 1.0f);
        colors[ImGuiCol_TabDimmedSelected] = colors[ImGuiCol_TabSelected];
        colors[ImGuiCol_Border] = runtimeBorder;
        colors[ImGuiCol_Separator] = runtimeBorder;
    }
}

void InitEditorFonts(ImGuiIO& io) {
    io.Fonts->Clear();

    float fontSize = 15.0f;
    bool fontLoaded = false;

#ifdef _WIN32
    char winDir[MAX_PATH] = {0};
    if (GetWindowsDirectoryA(winDir, MAX_PATH)) {
        char fontPath[MAX_PATH];
        snprintf(fontPath, MAX_PATH, "%s\\Fonts\\segoeui.ttf", winDir);

        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        cfg.PixelSnapH = true;

        ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, &cfg);
        if (font) {
            fontLoaded = true;
        }
    }
#endif

    if (!fontLoaded) {
        ImFontConfig cfg;
        cfg.SizePixels = 14.0f;
        cfg.OversampleH = 2;
        io.Fonts->AddFontDefault(&cfg);
    }
}

} // namespace Astral

