#include "debug_panel.hpp"
#include "json_editor.hpp"

#include <astral/app/app.hpp>

#include <imgui.h>

namespace astral {

namespace {

class RenderPanel final : public DebugPanel {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Render"; }

    void draw(Context& ctx, DebugPanelState&, bool& open) override {
        ImGui::SetNextWindowPos(ImVec2(10, 460), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Render", &open)) {
            if (ctx.renderer) {
                Json settings = to_json(ctx.renderer->settings(), {});
                if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen) && edit_json("settings", settings)) {
                    RenderSettings s = ctx.renderer->settings();
                    from_json(settings, s, {});
                    ctx.renderer->set_settings(s);
                }
                if (ImGui::Button("Reset history")) ctx.renderer->reset_history();
            }
            Json sun = to_json(ctx.sun, {});
            if (ImGui::CollapsingHeader("Sun") && edit_json("sun", sun)) from_json(sun, ctx.sun, {});
            Json env = to_json(ctx.environment, {});
            if (ImGui::CollapsingHeader("Environment") && edit_json("environment", env)) from_json(env, ctx.environment, {});
        }
        ImGui::End();
    }
};

} // namespace

std::unique_ptr<DebugPanel> make_render_panel() { return std::make_unique<RenderPanel>(); }

} // namespace astral
