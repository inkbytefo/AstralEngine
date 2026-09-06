#include "Astral/Editor/EditorTheme.hpp"
#include "Astral/Editor/Gizmo/ViewportGizmoToolbar.hpp"

#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdio>

namespace Astral {

namespace {
struct ToolButton { const char* label; const char* tooltip; GizmoOperation operation; };
constexpr std::array<ToolButton, 5> kTools{{
    {"Q", "Secim modu (Q)", GizmoOperation::Select},
    {"W", "Tasi (W)", GizmoOperation::Translate},
    {"E", "Dondur (E)", GizmoOperation::Rotate},
    {"R", "Olcekle (R)", GizmoOperation::Scale},
    {"T", "Evrensel donusum (T)", GizmoOperation::Universal}
}};
}

ViewportTransportAction ViewportGizmoToolbar::Draw(GizmoState& state, const glm::vec2& origin,
                                const glm::vec2& size, const glm::mat4& view, bool isPlaying) {
    ViewportTransportAction action = ViewportTransportAction::None;
    // A full-width reserved row, never an overlay over the rendered scene.
    ImGui::SetCursorScreenPos({origin.x, origin.y});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorPalette::Canvas);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    if (ImGui::BeginChild("ViewportGizmoToolbar", ImVec2(size.x, 44), ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::BeginDisabled(isPlaying);
        for (const auto& tool : kTools) {
            ImGui::PushStyleColor(ImGuiCol_Button, state.operation == tool.operation ? EditorPalette::Accent : EditorPalette::Raised);
            if (ImGui::Button(tool.label, ImVec2(28, 28))) state.operation = tool.operation;
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tool.tooltip);
            ImGui::SameLine(0, 4);
        }
        if (ImGui::Button(GizmoSpaceName(state.space), ImVec2(58, 28))) state.ToggleSpace();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("World / Local koordinat alani");
        ImGui::EndDisabled();
        // Keep all transport actions accessible when the viewport is narrow.
        ImGui::SameLine(0, 12);
        if (size.x > 680) ImGui::SetCursorPosX((size.x - 192.0f) * 0.5f);
        ImGui::BeginDisabled(isPlaying);
        ImGui::PushStyleColor(ImGuiCol_Button, EditorPalette::Accent);
        if (ImGui::Button(">##Play", ImVec2(42, 28))) action = ViewportTransportAction::Play;
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play (F5)");
        ImGui::SameLine(0, 4);
        ImGui::BeginDisabled(!isPlaying);
        if (ImGui::Button("||##Pause", ImVec2(42, 28))) action = ViewportTransportAction::Pause;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause / Devam et (F6)");
        ImGui::SameLine(0, 4);
        if (ImGui::Button("[]##Stop", ImVec2(42, 28))) action = ViewportTransportAction::Stop;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop (F5)");
        ImGui::EndDisabled();        if (size.x > 490) {
            DrawOrientationIndicator(ImGui::GetWindowDrawList(), {origin.x + size.x - 24, origin.y + 22}, 18, view);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    return action;
}

void ViewportGizmoToolbar::DrawContext(GizmoState& state, const glm::vec2& origin, const glm::vec2& size) {
    if (!state.IsEnabled() || size.x < 160 || size.y < 220) return;
    ImGui::SetCursorScreenPos({origin.x + 8, origin.y + 8});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorPalette::Canvas);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 8));
    if (ImGui::BeginChild("ToolContext", ImVec2(66, 190), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::TextDisabled("SNAP");
        ImGui::Separator();
        float* value = &state.translationSnap;
        std::array<float, 3> steps{0.1f, 0.5f, 1.0f};
        const char* unit = "metre";
        if (state.operation == GizmoOperation::Rotate) {
            value = &state.rotationSnapDegrees;
            steps = {15, 45, 90};
            unit = "derece";
        } else if (state.operation == GizmoOperation::Scale) {
            value = &state.scaleSnap;
            steps = {0.1f, 0.25f, 0.5f};
            unit = "olcek";
        }
        for (float step : steps) {
            char label[16];
            std::snprintf(label, sizeof(label), "%g", step);
            ImGui::PushStyleColor(ImGuiCol_Button, *value == step ? EditorPalette::Accent : EditorPalette::Raised);
            if (ImGui::Button(label, ImVec2(-1, 26))) *value = step;
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap: %g %s. Uygulamak icin Ctrl basili tutun.", step, unit);
        }
        ImGui::TextDisabled("%s", unit);
        ImGui::TextDisabled("Ctrl");
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}
void ViewportGizmoToolbar::DrawOrientationIndicator(ImDrawList* drawList, const glm::vec2& center,
                                                     float radius, const glm::mat4& view) const {
    if (!drawList) return;
    const ImVec2 c(center.x, center.y);
    drawList->AddCircleFilled(c, radius, IM_COL32(23, 25, 28, 255), 32);
    drawList->AddCircle(c, radius, IM_COL32(75, 84, 100, 180), 32, 1.0f);

    struct Axis { const char* label; glm::vec3 direction; ImU32 color; float depth; glm::vec2 point; bool positive; };
    std::array<Axis, 6> axes{{
        {"X", { 1, 0, 0}, IM_COL32(235, 64, 70, 255), 0, {}, true},
        {"",  {-1, 0, 0}, IM_COL32(135, 48, 54, 180), 0, {}, false},
        {"Y", {0,  1, 0}, IM_COL32(70, 215, 105, 255), 0, {}, true},
        {"",  {0, -1, 0}, IM_COL32(48, 130, 72, 180), 0, {}, false},
        {"Z", {0, 0,  1}, IM_COL32(60, 145, 245, 255), 0, {}, true},
        {"",  {0, 0, -1}, IM_COL32(45, 86, 148, 180), 0, {}, false}
    }};
    const glm::mat3 rotation(view);
    for (auto& axis : axes) {
        const glm::vec3 projected = rotation * axis.direction;
        axis.depth = projected.z;
        axis.point = {center.x + projected.x * radius * 0.63f, center.y - projected.y * radius * 0.63f};
    }
    std::sort(axes.begin(), axes.end(), [](const Axis& a, const Axis& b) { return a.depth < b.depth; });
    for (const auto& axis : axes) {
        const ImVec2 p(axis.point.x, axis.point.y);
        const ImU32 color = axis.depth >= 0.0f ? axis.color : IM_COL32(76, 82, 94, 135);
        drawList->AddLine(c, p, color, axis.depth >= 0.0f ? 2.0f : 1.0f);
        const float nodeRadius = axis.depth >= 0.0f && axis.positive ? 7.0f : 3.5f;
        drawList->AddCircleFilled(p, nodeRadius, color, 14);
        if (axis.depth >= 0.0f && axis.positive) {
            const ImVec2 textSize = ImGui::CalcTextSize(axis.label);
            drawList->AddText({p.x - textSize.x * 0.5f, p.y - textSize.y * 0.5f}, IM_COL32_WHITE, axis.label);
        }
    }
    drawList->AddCircleFilled(c, 2.5f, IM_COL32(205, 212, 224, 230), 12);
}

} // namespace Astral



