#include "theme/editor_theme.hpp"

#include <cmath>

namespace astral::editor {

const EditorThemeTokens& editor_theme() noexcept {
    static const EditorThemeTokens tokens{};
    return tokens;
}

void apply_editor_theme() {
    const EditorThemeTokens& t = editor_theme();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.ChildRounding = t.rounding;
    s.FrameRounding = t.rounding;
    s.PopupRounding = t.rounding;
    s.ScrollbarRounding = t.rounding;
    s.GrabRounding = t.rounding;
    s.TabRounding = t.rounding;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.PopupBorderSize = 1.0f;
    s.WindowPadding = ImVec2(10.0f, 8.0f);
    s.FramePadding = ImVec2(8.0f, 5.0f);
    s.ItemSpacing = ImVec2(7.0f, 6.0f);
    s.IndentSpacing = 16.0f;
    s.ScrollbarSize = 10.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = t.text_primary;
    c[ImGuiCol_TextDisabled] = t.text_muted;
    c[ImGuiCol_WindowBg] = t.bg_surface;
    c[ImGuiCol_ChildBg] = t.bg_base;
    c[ImGuiCol_PopupBg] = t.bg_elevated;
    c[ImGuiCol_Border] = t.border;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = t.bg_base;
    c[ImGuiCol_FrameBgHovered] = t.bg_hover;
    c[ImGuiCol_FrameBgActive] = t.bg_elevated;
    c[ImGuiCol_TitleBg] = t.bg_base;
    c[ImGuiCol_TitleBgActive] = t.bg_surface;
    c[ImGuiCol_TitleBgCollapsed] = t.bg_base;
    c[ImGuiCol_MenuBarBg] = t.bg_base;
    c[ImGuiCol_ScrollbarBg] = t.bg_base;
    c[ImGuiCol_ScrollbarGrab] = t.bg_elevated;
    c[ImGuiCol_ScrollbarGrabHovered] = t.bg_hover;
    c[ImGuiCol_ScrollbarGrabActive] = t.accent;
    c[ImGuiCol_CheckMark] = t.accent_hover;
    c[ImGuiCol_SliderGrab] = t.accent;
    c[ImGuiCol_SliderGrabActive] = t.accent_hover;
    c[ImGuiCol_Button] = t.bg_elevated;
    c[ImGuiCol_ButtonHovered] = t.bg_hover;
    c[ImGuiCol_ButtonActive] = t.accent_active;
    c[ImGuiCol_Header] = ImVec4(t.accent.x, t.accent.y, t.accent.z, 0.24f);
    c[ImGuiCol_HeaderHovered] = ImVec4(t.accent.x, t.accent.y, t.accent.z, 0.36f);
    c[ImGuiCol_HeaderActive] = ImVec4(t.accent.x, t.accent.y, t.accent.z, 0.50f);
    c[ImGuiCol_Separator] = t.border;
    c[ImGuiCol_SeparatorHovered] = t.accent;
    c[ImGuiCol_SeparatorActive] = t.accent_hover;
    c[ImGuiCol_ResizeGrip] = ImVec4(t.border.x, t.border.y, t.border.z, 0.4f);
    c[ImGuiCol_ResizeGripHovered] = t.accent;
    c[ImGuiCol_ResizeGripActive] = t.accent_hover;
    c[ImGuiCol_Tab] = t.bg_base;
    c[ImGuiCol_TabHovered] = t.bg_hover;
    c[ImGuiCol_TabSelected] = t.bg_elevated;
    c[ImGuiCol_TabSelectedOverline] = t.accent;
    c[ImGuiCol_TabDimmed] = t.bg_base;
    c[ImGuiCol_TabDimmedSelected] = t.bg_surface;
#ifdef IMGUI_HAS_DOCK
    c[ImGuiCol_DockingPreview] = ImVec4(t.accent.x, t.accent.y, t.accent.z, 0.45f);
    c[ImGuiCol_DockingEmptyBg] = t.bg_base;
#endif
    c[ImGuiCol_TableHeaderBg] = t.bg_elevated;
    c[ImGuiCol_TableBorderStrong] = t.border;
    c[ImGuiCol_TableBorderLight] = ImVec4(t.border.x, t.border.y, t.border.z, 0.5f);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.02f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(t.accent.x, t.accent.y, t.accent.z, 0.40f);
    c[ImGuiCol_NavCursor] = t.accent_hover;
}

namespace {

void draw_vector_icon(ImDrawList* dl, ImVec2 c, float r, RailIcon icon, ImU32 col) {
    const float t = 1.6f;
    switch (icon) {
    case RailIcon::Workspace:
        dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 2.0f, t);
        dl->AddLine(ImVec2(c.x - r * 0.25f, c.y - r), ImVec2(c.x - r * 0.25f, c.y + r), col, t);
        dl->AddLine(ImVec2(c.x - r * 0.25f, c.y + r * 0.2f), ImVec2(c.x + r, c.y + r * 0.2f), col, t);
        break;
    case RailIcon::Assets:
        dl->AddRect(ImVec2(c.x - r, c.y - r * 0.55f), ImVec2(c.x + r, c.y + r * 0.85f), col, 2.0f, t);
        dl->AddLine(ImVec2(c.x - r, c.y - r * 0.55f), ImVec2(c.x - r * 0.75f, c.y - r * 0.9f), col, t);
        dl->AddLine(ImVec2(c.x - r * 0.75f, c.y - r * 0.9f), ImVec2(c.x - r * 0.1f, c.y - r * 0.9f), col, t);
        dl->AddLine(ImVec2(c.x - r * 0.1f, c.y - r * 0.9f), ImVec2(c.x + r * 0.1f, c.y - r * 0.55f), col, t);
        break;
    case RailIcon::World:
        dl->AddCircle(c, r, col, 20, t);
        dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, t * 0.85f);
        dl->AddEllipse(c, ImVec2(r * 0.45f, r), col, 0.0f, 16, t * 0.85f);
        break;
    case RailIcon::Tools:
        dl->AddLine(ImVec2(c.x - r, c.y - r * 0.5f), ImVec2(c.x + r, c.y - r * 0.5f), col, t);
        dl->AddLine(ImVec2(c.x - r, c.y + r * 0.5f), ImVec2(c.x + r, c.y + r * 0.5f), col, t);
        dl->AddCircleFilled(ImVec2(c.x - r * 0.3f, c.y - r * 0.5f), 2.8f, col);
        dl->AddCircleFilled(ImVec2(c.x + r * 0.35f, c.y + r * 0.5f), 2.8f, col);
        break;
    case RailIcon::Agent: {
        // Minimal 4-point diamond star (calm, editorial)
        const ImVec2 pts[4] = {{c.x, c.y - r}, {c.x + r * 0.85f, c.y}, {c.x, c.y + r}, {c.x - r * 0.85f, c.y}};
        dl->AddPolyline(pts, 4, col, t, ImDrawFlags_Closed);
        dl->AddCircleFilled(c, 2.2f, col);
        break;
    }
    case RailIcon::Inspector:
        dl->AddRect(ImVec2(c.x - r * 0.85f, c.y - r), ImVec2(c.x + r * 0.85f, c.y + r), col, 2.0f, t);
        dl->AddLine(ImVec2(c.x - r * 0.45f, c.y - r * 0.4f), ImVec2(c.x + r * 0.45f, c.y - r * 0.4f), col, t);
        dl->AddLine(ImVec2(c.x - r * 0.45f, c.y), ImVec2(c.x + r * 0.45f, c.y), col, t);
        dl->AddLine(ImVec2(c.x - r * 0.45f, c.y + r * 0.4f), ImVec2(c.x + r * 0.15f, c.y + r * 0.4f), col, t);
        break;
    }
}

} // namespace

bool draw_rail_icon_button(const char* id, RailIcon icon, const char* tooltip, bool active, float size) {
    const EditorThemeTokens& t = editor_theme();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (active) {
        dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size),
                          ImGui::ColorConvertFloat4ToU32(ImVec4(t.accent.x, t.accent.y, t.accent.z, 0.22f)), 5.0f);
        dl->AddRectFilled(ImVec2(pos.x - 4.0f, pos.y + 6.0f), ImVec2(pos.x - 1.5f, pos.y + size - 6.0f),
                          ImGui::ColorConvertFloat4ToU32(t.accent_hover), 2.0f);
    } else if (hovered) {
        dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), ImGui::ColorConvertFloat4ToU32(t.bg_hover), 5.0f);
    }
    const ImVec4 icon_col = active ? t.text_primary : (hovered ? t.text_primary : t.text_muted);
    draw_vector_icon(dl, ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f), size * 0.26f, icon,
                     ImGui::ColorConvertFloat4ToU32(icon_col));
    if (hovered && tooltip) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

void draw_status_pill(const char* label, ImVec4 dot_color) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetFrameHeight();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(ImVec2(p.x + 6.0f, p.y + h * 0.5f), 3.5f, ImGui::ColorConvertFloat4ToU32(dot_color));
    ImGui::Dummy(ImVec2(12.0f, h));
    ImGui::SameLine(0.0f, 3.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
}

} // namespace astral::editor
