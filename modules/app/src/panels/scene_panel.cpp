#include "debug_panel.hpp"

#include <astral/app/app.hpp>
#include <astral/world/scene.hpp>

#include <imgui.h>

#include <cstdio>
#include <string>
#include <vector>

namespace astral {

namespace {

class ScenePanel final : public DebugPanel {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Scene"; }
    [[nodiscard]] bool default_open() const noexcept override { return true; }

    void draw(Context& ctx, DebugPanelState& state, bool& open) override {
        World& world = ctx.world;
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 330, 32), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Scene", &open)) {
            char buffer[128];
            std::snprintf(buffer, sizeof buffer, "%s", filter_.c_str());
            if (ImGui::InputTextWithHint("##filter", "filter by name", buffer, sizeof buffer)) filter_ = buffer;
            std::vector<Entity> shown;
            world.for_each_entity([&](Entity e) {
                if (world.has<PrefabPart>(e)) return; // parts are listed under their instance in the inspector
                if (!filter_.empty() && entity_label(world, e).find(filter_) == std::string::npos) return;
                shown.push_back(e);
            });
            ImGui::TextDisabled("%zu entities", shown.size());
            if (ImGui::BeginChild("list")) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(shown.size()));
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const Entity e = shown[static_cast<std::size_t>(i)];
                        ImGui::PushID(static_cast<int>(e.index));
                        if (ImGui::Selectable(entity_label(world, e).c_str(), e == state.selected)) state.selected = e;
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

private:
    std::string filter_;
};

} // namespace

std::unique_ptr<DebugPanel> make_scene_panel() { return std::make_unique<ScenePanel>(); }

} // namespace astral
