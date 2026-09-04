#include "Astral/Editor/EditorStatusBar.hpp"

#include <cstdio>
#include <algorithm>

namespace Astral {

void DrawEditorStatusBar(const StatusBarInfo& info) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    float barHeight = 24.0f;
    ImVec2 barPos = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - barHeight);
    ImVec2 barSize = ImVec2(viewport->WorkSize.x, barHeight);

    ImGui::SetNextWindowPos(barPos);
    ImGui::SetNextWindowSize(barSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.118f, 0.118f, 0.118f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.600f, 0.600f, 0.600f, 1.0f));

    ImGui::Begin("##AstralStatusBar", nullptr, flags);

    // FPS
    float fps = 0.0f;
    float maxMs = std::max(info.gpuTimeMs, info.cpuTimeMs);
    if (maxMs > 0.001f) {
        fps = 1000.0f / maxMs;
    }

    char fpsText[32];
    snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", fps);
    ImGui::Text("%s", fpsText);

    ImGui::SameLine(0, 16.0f);
    ImGui::TextUnformatted("|");
    ImGui::SameLine(0, 16.0f);

    // GPU Time
    char gpuText[64];
    snprintf(gpuText, sizeof(gpuText), "GPU: %.2f ms", info.gpuTimeMs);
    ImGui::Text("%s", gpuText);

    ImGui::SameLine(0, 16.0f);
    ImGui::TextUnformatted("|");
    ImGui::SameLine(0, 16.0f);

    // CPU Time
    char cpuText[64];
    snprintf(cpuText, sizeof(cpuText), "CPU: %.2f ms", info.cpuTimeMs);
    ImGui::Text("%s", cpuText);

    ImGui::SameLine(0, 16.0f);
    ImGui::TextUnformatted("|");
    ImGui::SameLine(0, 16.0f);

    // Entity Count
    char entityText[64];
    snprintf(entityText, sizeof(entityText), "Varliklar: %zu", info.entityCount);
    ImGui::Text("%s", entityText);

    ImGui::SameLine(0, 16.0f);
    ImGui::TextUnformatted("|");
    ImGui::SameLine(0, 16.0f);

    // Engine features
    ImGui::Text("Vulkan 1.4");

    ImGui::SameLine(0, 12.0f);

    // Grid status
    ImGui::PushStyleColor(ImGuiCol_Text, info.gridEnabled
        ? ImVec4(0.3f, 0.75f, 0.4f, 1.0f)   // green
        : ImVec4(0.6f, 0.35f, 0.35f, 1.0f)); // red-ish
    ImGui::Text("Grid: %s", info.gridEnabled ? "ON" : "OFF");
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 12.0f);

    // TAA status
    ImGui::PushStyleColor(ImGuiCol_Text, info.taaEnabled
        ? ImVec4(0.3f, 0.75f, 0.4f, 1.0f)
        : ImVec4(0.6f, 0.35f, 0.35f, 1.0f));
    ImGui::Text("TAA: %s", info.taaEnabled ? "ON" : "OFF");
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

} // namespace Astral
