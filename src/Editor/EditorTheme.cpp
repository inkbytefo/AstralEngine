#include "Astral/Editor/EditorTheme.hpp"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Astral {

void ApplyAstralTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // ── Geometry (Photoshop-style sharp panels) ─────────────────
    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = 2.0f;
    style.GrabRounding      = 2.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 2.0f;
    style.TabRounding       = 4.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.ItemSpacing       = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 8.0f;

    style.WindowTitleAlign   = ImVec2(0.0f, 0.5f);
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);

    // ── Astral Engine Color Palette ─────────────────────────────
    // Derived from Photoshop 2026 Dark Theme with Astral blue accent
    ImVec4 bg_canvas       = ImVec4(0.118f, 0.118f, 0.118f, 1.0f);  // #1e1e1e
    ImVec4 bg_panel        = ImVec4(0.176f, 0.176f, 0.176f, 1.0f);  // #2d2d2d
    ImVec4 bg_popup        = ImVec4(0.157f, 0.157f, 0.157f, 1.0f);  // #282828
    ImVec4 bg_header       = ImVec4(0.118f, 0.118f, 0.118f, 1.0f);  // #1e1e1e
    ImVec4 bg_menubar      = ImVec4(0.204f, 0.204f, 0.204f, 1.0f);  // #343434
    ImVec4 bg_input        = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);  // #232323
    ImVec4 bg_hover        = ImVec4(0.243f, 0.243f, 0.243f, 1.0f);  // #3e3e3e
    ImVec4 bg_active       = ImVec4(0.278f, 0.278f, 0.278f, 1.0f);  // #474747
    ImVec4 bg_tab_active   = ImVec4(0.176f, 0.176f, 0.176f, 1.0f);  // #2d2d2d
    ImVec4 bg_tab_inactive = ImVec4(0.137f, 0.137f, 0.137f, 1.0f);  // #232323

    ImVec4 accent          = ImVec4(0.176f, 0.365f, 0.667f, 1.0f);  // #2d5daa  Astral blue
    ImVec4 accent_hover    = ImVec4(0.220f, 0.420f, 0.750f, 1.0f);  // #386bbf
    ImVec4 accent_active   = ImVec4(0.145f, 0.310f, 0.580f, 1.0f);  // #254f94

    ImVec4 border          = ImVec4(0.243f, 0.243f, 0.243f, 1.0f);  // #3e3e3e
    ImVec4 border_shadow   = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    ImVec4 text_primary    = ImVec4(0.800f, 0.800f, 0.800f, 1.0f);  // #cccccc
    ImVec4 text_disabled   = ImVec4(0.502f, 0.502f, 0.502f, 1.0f);  // #808080

    ImVec4 scrollbar_grab  = ImVec4(0.325f, 0.325f, 0.325f, 1.0f);  // #535353
    ImVec4 scrollbar_hover = ImVec4(0.400f, 0.400f, 0.400f, 1.0f);  // #666666
    ImVec4 scrollbar_active= ImVec4(0.502f, 0.502f, 0.502f, 1.0f);  // #808080

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

    colors[ImGuiCol_Button]                 = ImVec4(0.204f, 0.204f, 0.204f, 1.0f);
    colors[ImGuiCol_ButtonHovered]          = bg_hover;
    colors[ImGuiCol_ButtonActive]           = accent_active;

    colors[ImGuiCol_Header]                 = ImVec4(0.220f, 0.220f, 0.220f, 1.0f);
    colors[ImGuiCol_HeaderHovered]          = bg_hover;
    colors[ImGuiCol_HeaderActive]           = accent_active;

    colors[ImGuiCol_Separator]              = ImVec4(0.157f, 0.157f, 0.157f, 1.0f);
    colors[ImGuiCol_SeparatorHovered]       = accent_hover;
    colors[ImGuiCol_SeparatorActive]        = accent_active;

    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ResizeGripHovered]      = bg_hover;
    colors[ImGuiCol_ResizeGripActive]       = accent_active;

    colors[ImGuiCol_Tab]                    = bg_tab_inactive;
    colors[ImGuiCol_TabHovered]             = bg_hover;
    colors[ImGuiCol_TabActive]              = bg_tab_active;
    colors[ImGuiCol_TabUnfocused]           = bg_tab_inactive;
    colors[ImGuiCol_TabUnfocusedActive]     = bg_tab_active;

    colors[ImGuiCol_DockingPreview]         = accent_hover;
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
