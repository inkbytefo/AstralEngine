#include "editor_state.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace astral::editor {

// Panel factory declarations from sibling panel translation units.
std::unique_ptr<EditorPanel> make_viewport_panel();
std::unique_ptr<EditorPanel> make_scene_tree_panel();
std::unique_ptr<EditorPanel> make_inspector_panel();
std::unique_ptr<EditorPanel> make_content_shelf_panel();
std::unique_ptr<EditorPanel> make_agent_panel();

namespace {

class ConsolePanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "console"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "Console"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext& ec) const noexcept override { return ec.state.vis.console; }

    void draw(EditorPanelContext& ec) override {
        if (ImGui::Begin("Console", &ec.state.vis.console)) {
            if (ImGui::SmallButton("Clear")) ec.state.ui.console.clear();
            ImGui::Separator();
            ImGui::BeginChild("##ConsoleScroll");
            for (const std::string& line : ec.state.ui.console) ImGui::TextUnformatted(line.c_str());
            ImGui::EndChild();
        }
        ImGui::End();
    }
};

class ProfilerPanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "profiler"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "Profiler"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext& ec) const noexcept override { return ec.state.vis.profiler; }

    void draw(EditorPanelContext& ec) override {
        if (!ImGui::Begin("Profiler", &ec.state.vis.profiler)) {
            ImGui::End();
            return;
        }
        if (ec.app.profiler) {
            const std::vector<float> times = ec.app.profiler->frame_times();
            float avg = 0.0f, worst = 0.0f;
            for (float v : times) avg += v, worst = std::max(worst, v);
            if (!times.empty()) avg /= static_cast<float>(times.size());
            ImGui::Text("CPU: %.2f ms avg | %.2f ms worst", static_cast<double>(avg), static_cast<double>(worst));
            if (!times.empty()) {
                ImGui::PlotLines("##FramePlot", times.data(), static_cast<int>(times.size()), 0, nullptr, 0.0f,
                                 std::max(33.3f, worst), ImVec2(-1.0f, 52.0f));
            }
            if (ImGui::CollapsingHeader("CPU Scopes", ImGuiTreeNodeFlags_DefaultOpen) &&
                ImGui::BeginTable("##CpuScopes", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                for (const ProfileSample& s : ec.app.profiler->last_frame()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(s.name);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f ms", static_cast<double>(s.ms));
                }
                ImGui::EndTable();
            }
        }
        if (ec.app.renderer && ImGui::CollapsingHeader("GPU Passes", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("GPU Total: %.3f ms", static_cast<double>(ec.app.renderer->gpu_frame_ms()));
            const RenderStats& st = ec.app.renderer->stats();
            ImGui::TextDisabled("%s | %u draws | %u inst | %u lights | %u passes",
                                st.ray_traced ? "Ray Traced" : "Raster", st.draw_calls, st.instances,
                                st.point_lights + st.rect_lights, st.passes);
            if (ImGui::BeginTable("##GpuPasses", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                for (const PassTiming& pt : ec.app.renderer->gpu_timings()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(pt.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f ms", static_cast<double>(pt.ms));
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
};

class RenderSettingsPanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "render"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "Render"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext& ec) const noexcept override {
        return ec.state.vis.render_settings;
    }

    void draw(EditorPanelContext& ec) override {
        if (!ImGui::Begin("Render", &ec.state.vis.render_settings)) {
            ImGui::End();
            return;
        }
        if (ec.app.renderer) {
            RenderSettings rs = ec.app.renderer->settings();
            bool changed = false;
            changed |= ImGui::Checkbox("Ray Tracing", &rs.ray_tracing);
            changed |= ImGui::Checkbox("Path Tracer (Ref)", &rs.path_tracer);
            changed |= ImGui::Checkbox("ReSTIR DI", &rs.restir_di);
            changed |= ImGui::SliderFloat("Bloom", &rs.bloom_strength, 0.0f, 0.3f);
            changed |= ImGui::SliderFloat("TAA Blend", &rs.taa_blend, 0.02f, 0.5f);
            if (changed) ec.app.renderer->set_settings(rs);
            if (ImGui::Button("Reset Temporal History")) ec.app.renderer->reset_history();
        }
        ImGui::SeparatorText("Lighting & Environment");
        ImGui::DragFloat3("Sun Direction", &ec.app.sun.direction.x, 0.02f);
        ImGui::DragFloat("Sun Intensity", &ec.app.sun.intensity, 0.05f, 0.0f, 20.0f);
        ImGui::ColorEdit3("Sky Color", &ec.app.environment.sky.x);
        ImGui::SliderFloat("Ambient", &ec.app.environment.ambient, 0.0f, 2.0f);
        ImGui::SliderFloat("Exposure", &ec.app.environment.exposure, 0.1f, 4.0f);
        ImGui::SeparatorText("Debug Overlays");
        ImGui::Checkbox("Physics Shapes", &ec.app.debug_show.physics);
        ImGui::Checkbox("Lights", &ec.app.debug_show.lights);
        ImGui::Checkbox("World Cells", &ec.app.debug_show.world_cells);
        ImGui::End();
    }
};

class MaterialsOverviewPanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "materials"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "Materials"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext& ec) const noexcept override { return ec.state.vis.materials; }

    void draw(EditorPanelContext& ec) override {
        if (!ImGui::Begin("Materials", &ec.state.vis.materials)) {
            ImGui::End();
            return;
        }
        if (const std::string_view err = ec.app.materials().shader_error(); !err.empty()) {
            ImGui::TextColored(ImVec4(0.9f, 0.45f, 0.35f, 1.0f), "Shader Error:");
            ImGui::TextWrapped("%.*s", static_cast<int>(err.size()), err.data());
            ImGui::Separator();
        }
        ImGui::TextDisabled("Project Materials & Shaders");
        for (const std::filesystem::path& p : ec.state.ui.asset_files) {
            const std::string ext = p.extension().string();
            if (ext != ".material" && ext != ".ashader") continue;
            ImGui::BulletText("%s", p.filename().string().c_str());
        }
        ImGui::End();
    }
};

class WorldAuthoringPanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "world_authoring"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "World Authoring"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext& ec) const noexcept override { return ec.state.vis.world_panel; }

    void draw(EditorPanelContext& ec) override {
        if (ImGui::Begin("World Authoring", &ec.state.vis.world_panel)) {
            draw_world_authoring_contents(ec);
        }
        ImGui::End();
    }
};

} // namespace

void register_editor_panels(PanelRegistry& registry) {
    registry.add(make_scene_tree_panel());
    registry.add(make_viewport_panel());
    registry.add(make_agent_panel());
    registry.add(make_inspector_panel());
    registry.add(make_content_shelf_panel());
    registry.add(std::make_unique<WorldAuthoringPanel>());
    registry.add(std::make_unique<ConsolePanel>());
    registry.add(std::make_unique<ProfilerPanel>());
    registry.add(std::make_unique<RenderSettingsPanel>());
    registry.add(std::make_unique<MaterialsOverviewPanel>());
}

} // namespace astral::editor
