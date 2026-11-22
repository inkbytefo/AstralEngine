#include "debug_panel.hpp"

#include <astral/app/app.hpp>

#include <imgui.h>

#include <algorithm>
#include <vector>

namespace astral {

namespace {

void draw_cpu_timings(const Profiler& profiler) {
    if (!ImGui::CollapsingHeader("CPU", ImGuiTreeNodeFlags_DefaultOpen)) return;
    if (!ImGui::BeginTable("cpu", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) return;
    for (const ProfileSample& s : profiler.last_frame()) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        // Indent(0) would mean "the default indent": only indent nested scopes.
        const float indent = static_cast<float>(s.depth) * 14.0f;
        if (indent > 0.0f) ImGui::Indent(indent);
        ImGui::TextUnformatted(s.name);
        if (indent > 0.0f) ImGui::Unindent(indent);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f ms", static_cast<double>(s.ms));
    }
    ImGui::EndTable();
}

void draw_gpu_timings(const Renderer& renderer) {
    if (!ImGui::CollapsingHeader("GPU", ImGuiTreeNodeFlags_DefaultOpen)) return;
    const auto timings = renderer.gpu_timings();
    if (timings.empty()) ImGui::TextDisabled("no timestamps yet");
    else ImGui::Text("total %.3f ms", static_cast<double>(renderer.gpu_frame_ms()));
    if (!timings.empty() && ImGui::BeginTable("gpu", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        for (const PassTiming& t : timings) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(t.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.3f ms", static_cast<double>(t.ms));
        }
        ImGui::EndTable();
    }
    const RenderStats& st = renderer.stats();
    ImGui::TextDisabled("%s | %u draws, %u instances, %u lights, %u SDF", st.ray_traced ? "ray traced" : "raster",
                        st.draw_calls, st.instances, st.point_lights + st.rect_lights, st.sdf_shapes);
}

class PerformancePanel final : public DebugPanel {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Performance"; }
    [[nodiscard]] bool default_open() const noexcept override { return true; }

    void draw(Context& ctx, DebugPanelState&, bool& open) override {
        ImGui::SetNextWindowPos(ImVec2(10, 32), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360, 420), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Performance", &open)) {
            const std::vector<float> times = ctx.profiler->frame_times();
            float average = 0.0f, worst = 0.0f;
            for (float t : times) {
                average += t;
                worst = std::max(worst, t);
            }
            average = times.empty() ? 0.0f : average / static_cast<float>(times.size());
            ImGui::Text("%.1f FPS  |  %.2f ms avg  |  %.2f ms worst", average > 0.0f ? 1000.0 / average : 0.0,
                        static_cast<double>(average), static_cast<double>(worst));
            if (!times.empty()) {
                ImGui::PlotLines("##frames", times.data(), static_cast<int>(times.size()), 0, nullptr, 0.0f,
                                 std::max(33.3f, worst), ImVec2(-1, 60));
            }
            draw_cpu_timings(*ctx.profiler);
            if (ctx.renderer) draw_gpu_timings(*ctx.renderer);
        }
        ImGui::End();
    }
};

} // namespace

std::unique_ptr<DebugPanel> make_performance_panel() { return std::make_unique<PerformancePanel>(); }

} // namespace astral
