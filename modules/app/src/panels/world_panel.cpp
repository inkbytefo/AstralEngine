#include "debug_panel.hpp"

#include <astral/app/app.hpp>

#include <imgui.h>

namespace astral {

namespace {

class WorldPanel final : public DebugPanel {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "World"; }

    void draw(Context& ctx, DebugPanelState&, bool& open) override {
        ImGui::SetNextWindowSize(ImVec2(300, 260), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("World", &open)) {
            const WorldPosition o = ctx.maps().world_origin();
            ImGui::Text("Origin: %.1f, %.1f, %.1f", o.x, o.y, o.z);
            if (const StreamingStats* st = ctx.maps().world_streaming()) {
                ImGui::Text("Cells: %u in world, %u loading, %u waiting", st->attached, st->loading, st->waiting);
                ImGui::Text("Last frame: +%u / -%u", st->attached_last_update, st->detached_last_update);
                ImGui::Checkbox("Draw cells", &ctx.debug_show.world_cells);
                if (MapLayers* layers = ctx.maps().world_layers(); layers && !layers->layers().empty()) {
                    ImGui::SeparatorText("Data layers");
                    for (const LayerDesc& l : layers->layers()) {
                        bool on = l.active;
                        if (ImGui::Checkbox(l.name.c_str(), &on)) layers->set(l.name, on);
                    }
                }
            } else {
                ImGui::TextDisabled("No map open (ctx.maps().open_world()).");
            }
        }
        ImGui::End();
    }
};

} // namespace

std::unique_ptr<DebugPanel> make_world_panel() { return std::make_unique<WorldPanel>(); }

} // namespace astral
