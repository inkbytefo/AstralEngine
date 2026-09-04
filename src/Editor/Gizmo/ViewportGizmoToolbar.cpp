#include "Astral/Editor/Gizmo/ViewportGizmoToolbar.hpp"

#include <imgui.h>
#include <algorithm>
#include <array>

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

void ViewportGizmoToolbar::Draw(GizmoState& state, const glm::vec2& origin,
                                const glm::vec2& size, const glm::mat4& view) {
    constexpr float indicatorRadius = 25.0f;
    const glm::vec2 indicatorCenter(origin.x + size.x - indicatorRadius - 14.0f,
                                    origin.y + indicatorRadius + 11.0f);
    DrawOrientationIndicator(ImGui::GetWindowDrawList(), indicatorCenter, indicatorRadius, view);

    constexpr float toolbarWidth = 264.0f;
    const glm::vec2 toolbarPos(indicatorCenter.x - indicatorRadius - toolbarWidth - 12.0f, origin.y + 10.0f);
    ImGui::SetCursorScreenPos(ImVec2(toolbarPos.x, toolbarPos.y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.085f, 0.105f, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 3.0f));
    if (ImGui::BeginChild("ViewportGizmoToolbar", ImVec2(toolbarWidth, 30.0f), true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        for (const auto& tool : kTools) {
            const bool active = state.operation == tool.operation;
            ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.16f, 0.42f, 0.82f, 1.0f)
                                                         : ImVec4(0.13f, 0.15f, 0.18f, 0.55f));
            if (ImGui::Button(tool.label, ImVec2(28.0f, 22.0f))) state.operation = tool.operation;
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tool.tooltip);
            ImGui::SameLine(0.0f, 3.0f);
        }
        ImGui::Dummy(ImVec2(2.0f, 0.0f));
        ImGui::SameLine(0.0f, 3.0f);
        if (ImGui::Button(GizmoSpaceName(state.space), ImVec2(58.0f, 22.0f))) state.ToggleSpace();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Koordinat alani: %s", GizmoSpaceName(state.space));
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void ViewportGizmoToolbar::DrawOrientationIndicator(ImDrawList* drawList, const glm::vec2& center,
                                                     float radius, const glm::mat4& view) const {
    if (!drawList) return;
    const ImVec2 c(center.x, center.y);
    drawList->AddCircleFilled(c, radius, IM_COL32(18, 21, 27, 220), 32);
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
