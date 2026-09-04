#include "Astral/Editor/Panels/Statistics.hpp"

#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace Astral {

void Statistics::Draw(float gpuTimeMs, float cpuTimeMs, size_t activeEntities) {
    ImGui::Begin("Motor Istatistikleri");

    // ── Section label ────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.600f, 0.600f, 0.600f, 1.0f));
    ImGui::Text("PERFORMANS");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4.0f));

    // ── Timing ───────────────────────────────────────────────
    ImGui::Text("GPU Raymarch: %.2f ms", gpuTimeMs);
    ImGui::Text("CPU Frame:    %.2f ms", cpuTimeMs);

    float fps = 0.0f;
    float maxMs = std::max(gpuTimeMs, cpuTimeMs);
    if (maxMs > 0.001f) {
        fps = 1000.0f / maxMs;
    } else {
        fps = ImGui::GetIO().Framerate;
    }
    ImGui::Text("FPS (Tahmini): %.1f", fps);

    ImGui::Dummy(ImVec2(0, 4.0f));

    // ── FPS graph (simple rolling buffer) ────────────────────
    static float fpsHistory[120] = {0};
    static int fpsHistoryOffset = 0;
    fpsHistory[fpsHistoryOffset] = fps;
    fpsHistoryOffset = (fpsHistoryOffset + 1) % 120;

    char fpsOverlay[32];
    snprintf(fpsOverlay, sizeof(fpsOverlay), "%.1f FPS", fps);

    ImGui::PushItemWidth(-1);
    ImGui::PlotLines("##FPSGraph", fpsHistory, 120, fpsHistoryOffset, fpsOverlay, 0.0f, 200.0f, ImVec2(0, 50));
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 4.0f));

    // ── Scene info ───────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.600f, 0.600f, 0.600f, 1.0f));
    ImGui::Text("SAHNE BILGISI");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4.0f));

    ImGui::Text("Aktif Varliklar: %zu", activeEntities);
    ImGui::Text("Vulkan API: 1.4 Dynamic Rendering");
    ImGui::Text("Compute Shader: SDF Raymarching");

    ImGui::End();
}

} // namespace Astral
