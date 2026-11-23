#pragma once
// Central UIStyle design tokens and crisp geometric vector icon helpers (ADR-0037).

#include <imgui.h>

#include <cstdint>

namespace astral::editor {

struct EditorThemeTokens {
    ImVec4 bg_base{0.067f, 0.071f, 0.086f, 1.0f};      // #111216
    ImVec4 bg_surface{0.090f, 0.098f, 0.122f, 1.0f};   // #17191F
    ImVec4 bg_elevated{0.122f, 0.133f, 0.165f, 1.0f};  // #1F222A
    ImVec4 bg_hover{0.165f, 0.176f, 0.216f, 1.0f};     // #2A2D37
    ImVec4 border{0.180f, 0.196f, 0.243f, 1.0f};       // #2E323E
    ImVec4 text_primary{0.902f, 0.910f, 0.933f, 1.0f}; // #E6E8EE
    ImVec4 text_muted{0.557f, 0.584f, 0.647f, 1.0f};   // #8E95A5
    ImVec4 accent{0.486f, 0.435f, 0.651f, 1.0f};       // #7C6FA6 muted violet
    ImVec4 accent_hover{0.573f, 0.522f, 0.741f, 1.0f}; // #9285BD
    ImVec4 accent_active{0.396f, 0.345f, 0.561f, 1.0f};
    ImVec4 status_ok{0.36f, 0.71f, 0.52f, 1.0f};
    ImVec4 status_warn{0.84f, 0.68f, 0.34f, 1.0f};
    ImVec4 status_err{0.82f, 0.38f, 0.40f, 1.0f};

    float rounding = 4.0f;
    float rail_width = 50.0f;
    float toolbar_height = 38.0f;
    float statusbar_height = 24.0f;
};

[[nodiscard]] const EditorThemeTokens& editor_theme() noexcept;
void apply_editor_theme();

enum class RailIcon : std::uint8_t { Workspace, Assets, World, Tools, Agent, Inspector };

/// Draws a crisp, vector-rendered rail icon button with tooltip and active indicator.
bool draw_rail_icon_button(const char* id, RailIcon icon, const char* tooltip, bool active, float size = 34.0f);

/// Draws a small colored dot + text status pill (shape + label, never color alone).
void draw_status_pill(const char* label, ImVec4 dot_color);

} // namespace astral::editor
