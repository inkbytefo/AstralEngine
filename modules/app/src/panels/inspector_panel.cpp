#include "debug_panel.hpp"
#include "json_editor.hpp"

#include <astral/app/app.hpp>
#include <astral/world/scene.hpp>

#include <imgui.h>

#include <string>
#include <vector>

namespace astral {

namespace {

void draw_prefab_parts(World& world, DebugPanelState& state, std::vector<Entity>& parts) {
    parts.clear();
    world.each<const PrefabPart>([&](Entity p, const PrefabPart& part) {
        if (part.instance == state.selected) parts.push_back(p);
    });
    if (parts.empty() || !ImGui::TreeNode("parts", "%zu prefab parts", parts.size())) return;
    for (Entity p : parts) {
        ImGui::PushID(static_cast<int>(p.index));
        if (ImGui::Selectable(entity_label(world, p).c_str())) state.selected = p;
        ImGui::PopID();
    }
    ImGui::TreePop();
}

void draw_components(Context& ctx, Entity e) {
    World& world = ctx.world;
    const TypeRegistry::Entry* remove = nullptr;
    for (const TypeRegistry::Entry& t : ctx.types.entries()) {
        if (!t.has(world, e)) continue;
        const std::string name(t.name);
        ImGui::PushID(name.c_str());
        const bool open =
            ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        ImGui::SameLine(ImGui::GetWindowWidth() - 30);
        if (ImGui::SmallButton("x")) remove = &t;
        if (open) {
            Json value = component_to_json(world, t, e);
            if (edit_json(name.c_str(), value)) component_from_json(world, t, e, value);
        }
        ImGui::PopID();
    }
    if (remove) remove->remove(world, e);
}

void draw_add_component(Context& ctx, Entity e) {
    if (!ImGui::BeginCombo("##add", "Add component")) return;
    for (const TypeRegistry::Entry& t : ctx.types.entries()) {
        if (t.has(ctx.world, e)) continue;
        if (ImGui::Selectable(std::string(t.name).c_str())) component_from_json(ctx.world, t, e, Json::object());
    }
    ImGui::EndCombo();
}

class InspectorPanel final : public DebugPanel {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Inspector"; }
    [[nodiscard]] bool default_open() const noexcept override { return true; }

    void draw(Context& ctx, DebugPanelState& state, bool& open) override {
        World& world = ctx.world;
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 330, 340), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 380), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Inspector", &open)) {
            if (!state.selected.valid()) {
                ImGui::TextDisabled("select an entity in Scene");
            } else {
                ImGui::Text("%s", entity_label(world, state.selected).c_str());
                ImGui::TextDisabled("index %u, generation %u", state.selected.index, state.selected.generation);
                if (const PrefabInstance* pi = world.get<const PrefabInstance>(state.selected)) {
                    ImGui::TextDisabled("prefab %s", pi->path.c_str());
                }
                draw_prefab_parts(world, state, parts_);
                const Entity e = state.selected;
                draw_components(ctx, e);
                draw_add_component(ctx, e);
                if (ImGui::Button("Destroy entity")) {
                    world.destroy(e);
                    state.selected = {};
                }
            }
        }
        ImGui::End();
    }

private:
    std::vector<Entity> parts_;
};

} // namespace

std::unique_ptr<DebugPanel> make_inspector_panel() { return std::make_unique<InspectorPanel>(); }

} // namespace astral
