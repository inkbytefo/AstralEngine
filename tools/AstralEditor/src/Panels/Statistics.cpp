#include "Astral/Editor/Panels/Statistics.hpp"
#include "Astral/Editor/EditorTheme.hpp"
#include <algorithm>
#include <cstdio>

namespace Astral {
namespace {
float EstimatedFps(float gpu, float cpu) {
    const float ms = std::max(gpu, cpu);
    return ms > 0.001f ? 1000.0f / ms : ImGui::GetIO().Framerate;
}
}

void Statistics::Update(float gpuTimeMs, float cpuTimeMs) {
    // Sample every frame, including while another tools tab is selected.
    m_FpsHistory[m_HistoryOffset] = EstimatedFps(gpuTimeMs, cpuTimeMs);
    m_HistoryOffset = (m_HistoryOffset + 1) % static_cast<int>(m_FpsHistory.size());
}

void Statistics::Draw(float gpuTimeMs, float cpuTimeMs, size_t activeEntities) {
    const float fps = EstimatedFps(gpuTimeMs, cpuTimeMs);
    ImGui::TextDisabled("PERFORMANS");
    const int columns = ImGui::GetContentRegionAvail().x < 480.0f ? 2 : 4;
    if (ImGui::BeginTable("Metrics", columns, ImGuiTableFlags_SizingStretchSame)) {
        const char* labels[] = {"FPS / TAHMINI", "GPU / ms", "CPU / ms", "VARLIKLAR"};
        char values[4][32];
        std::snprintf(values[0], 32, "%.1f", fps);
        std::snprintf(values[1], 32, "%.2f", gpuTimeMs);
        std::snprintf(values[2], 32, "%.2f", cpuTimeMs);
        std::snprintf(values[3], 32, "%zu", activeEntities);
        for (int i = 0; i < 4; ++i) {
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorPalette::Raised);
            ImGui::BeginChild("Metric", ImVec2(0, 64), ImGuiChildFlags_Borders);
            ImGui::TextDisabled("%s", labels[i]);
            ImGui::TextUnformatted(values[i]);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    const float peak = *std::max_element(m_FpsHistory.begin(), m_FpsHistory.end());
    ImGui::SetNextItemWidth(-1);
    ImGui::PlotLines("##FPSGraph", m_FpsHistory.data(), static_cast<int>(m_FpsHistory.size()),
                     m_HistoryOffset, "Kare hizi / son 120 kare", 0.0f, std::max(60.0f, peak * 1.1f), ImVec2(0, 60));
    ImGui::TextDisabled("Vulkan 1.4  /  Dynamic Rendering  /  SDF Raymarching");
}
} // namespace Astral
