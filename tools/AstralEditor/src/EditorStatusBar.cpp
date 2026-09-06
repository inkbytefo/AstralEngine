#include "Astral/Editor/EditorStatusBar.hpp"
#include "Astral/Editor/EditorWorkspace.hpp"
#include "Astral/Editor/EditorTheme.hpp"
#include <algorithm>
#include <cstdio>

namespace Astral {
void DrawEditorStatusBar(const StatusBarInfo& info) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float height = EditorStatusBarHeight();
    ImGui::SetNextWindowPos({viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - height});
    ImGui::SetNextWindowSize({viewport->WorkSize.x, height});
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorPalette::Canvas);
    ImGui::PushStyleColor(ImGuiCol_Text, EditorPalette::Muted);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("##AstralStatusBar", nullptr, flags)) {
        const ImVec2 pos = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddLine(pos, {pos.x + viewport->WorkSize.x, pos.y}, ImGui::GetColorU32(EditorPalette::Border));
        const float ms = std::max(info.gpuTimeMs, info.cpuTimeMs);
        const float fps = ms > 0.001f ? 1000.0f / ms : ImGui::GetIO().Framerate;
        ImGui::Text("%.1f FPS", fps);
        ImGui::SameLine(0, 20);
        ImGui::Text("GPU %.2f ms", info.gpuTimeMs);
        ImGui::SameLine(0, 20);
        ImGui::Text("CPU %.2f ms", info.cpuTimeMs);
        ImGui::SameLine(0, 20);
        ImGui::Text("%zu varlik", info.entityCount);
        char features[96];
        std::snprintf(features, sizeof(features), "Vulkan 1.4   |   Grid %s   |   TAA %s",
                      info.gridEnabled ? "ON" : "OFF", info.taaEnabled ? "ON" : "OFF");
        const float textWidth = ImGui::CalcTextSize(features).x;
        if (ImGui::GetContentRegionAvail().x > textWidth + 24) {
            ImGui::SameLine(ImGui::GetWindowWidth() - textWidth - 12);
            ImGui::TextUnformatted(features);
        } else if (ImGui::IsWindowHovered()) {
            ImGui::SetTooltip("%s", features);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}
} // namespace Astral
