#include "editor_state.hpp"

#include <imgui.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace astral::editor {

namespace {

void draw_layers_and_origin(EditorPanelContext& ec, WorldAuthoringSession& wa) {
    ImGui::Text("Map: %s (%zu source .ent)", wa.world_path().filename().string().c_str(), wa.raw_entities().size());
    if (ImGui::SmallButton("Reload")) {
        std::string err;
        (void)ec.workspace.reload_world_preview(ec.app, &err);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!wa.can_undo());
    if (ImGui::SmallButton("Undo")) {
        std::string err;
        if (wa.undo(&err)) (void)ec.workspace.reload_world_preview(ec.app, &err);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!wa.can_redo());
    if (ImGui::SmallButton("Redo")) {
        std::string err;
        if (wa.redo(&err)) (void)ec.workspace.reload_world_preview(ec.app, &err);
    }
    ImGui::EndDisabled();

    WorldPosition origin = wa.preview_origin();
    float orig3[3] = {static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z)};
    if (ImGui::DragFloat3("Floating Origin", orig3, 16.0f)) {
        wa.set_preview_origin({orig3[0], orig3[1], orig3[2]});
        std::string err;
        (void)ec.workspace.reload_world_preview(ec.app, &err);
    }

    if (ImGui::CollapsingHeader("Data Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        MapDesc updated = wa.map_desc();
        bool changed = false;
        for (LayerDesc& layer : updated.layers) {
            if (ImGui::Checkbox(layer.name.c_str(), &layer.active)) changed = true;
        }
        if (changed) {
            std::string err;
            (void)wa.save_map_desc(updated, &err);
            (void)ec.workspace.reload_world_preview(ec.app, &err);
        }
    }
}

void draw_cook_cells(const WorldAuthoringSession& wa) {
    if (!ImGui::CollapsingHeader("Cook Cells & Streaming", ImGuiTreeNodeFlags_DefaultOpen)) return;
    std::vector<std::string> errs;
    const std::vector<CellOwnershipInfo> cells = wa.inspect_cells(&errs);
    ImGui::TextDisabled("%zu active cells | %zu warnings", cells.size(), errs.size());
    for (const CellOwnershipInfo& c : cells) {
        ImGui::BulletText("%s [grid=%u (%d,%d)] %.0fm (%zu ents)", c.name.c_str(),
                          static_cast<unsigned>(c.key.grid), c.key.x, c.key.z, c.size, c.entity_count);
    }
}

void draw_level_instances(EditorPanelContext& ec, WorldAuthoringSession& wa, WorldAuthoringUiState& ui) {
    if (!ImGui::CollapsingHeader("Level Instances", ImGuiTreeNodeFlags_DefaultOpen)) return;
    std::string selected_scene;
    for (const MapEntity& me : wa.raw_entities()) {
        const auto parsed = parse_level_instance_entity(make_entity_file(me));
        if (!parsed) continue;
        char label[192];
        std::snprintf(label, sizeof label, "#%llu %s (%.0f, %.0f, %.0f)",
                      static_cast<unsigned long long>(parsed->instance_id), parsed->scene_path.c_str(),
                      parsed->position[0], parsed->position[1], parsed->position[2]);
        if (ImGui::Selectable(label, ui.selected_instance_id == parsed->instance_id)) {
            ui.selected_instance_id = parsed->instance_id;
        }
        if (ui.selected_instance_id == parsed->instance_id) selected_scene = parsed->scene_path;
    }
    if (ui.selected_instance_id == 0 || selected_scene.empty()) {
        ImGui::TextDisabled("Select a Level Instance to apply an Override or Edit-in-Context.");
        return;
    }
    auto edit_str = [](const char* name, std::string& s) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "%s", s.c_str());
        if (ImGui::InputText(name, buf, sizeof buf)) s = buf;
    };
    edit_str("Inner Entity Path", ui.edit_local_path);
    edit_str("Component", ui.edit_component);
    edit_str("Field", ui.edit_field);
    edit_str("Value", ui.edit_value);

    if (ImGui::Button("Instance Override")) {
        std::string err;
        if (wa.edit_instance_override(ui.selected_instance_id, ui.edit_local_path, ui.edit_component, ui.edit_field,
                                      Json(ui.edit_value), &err)) {
            (void)ec.workspace.reload_world_preview(ec.app, &err);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Edit-in-Context")) {
        const auto rec_id = static_cast<std::uint32_t>(std::strtoul(ui.edit_local_path.c_str(), nullptr, 10));
        std::string err;
        if (wa.edit_source_scene_in_context(selected_scene, rec_id, ui.edit_component, ui.edit_field,
                                            Json(ui.edit_value), &err)) {
            (void)ec.workspace.reload_world_preview(ec.app, &err);
        }
    }
}

void draw_placement_palette(EditorPanelContext& ec, WorldAuthoringSession& wa, WorldAuthoringUiState& ui) {
    if (!ImGui::CollapsingHeader("Placement Tools", ImGuiTreeNodeFlags_DefaultOpen)) return;
    for (std::size_t i = 0; i < ui.tools.size(); ++i) {
        const PlacementToolDef& tool = ui.tools[i];
        std::string label = "[" + std::string(placement_category_name(tool.category)) + "] " + tool.label;
        if (ImGui::Selectable(label.c_str(), ui.selected_tool == i)) ui.selected_tool = i;
    }
    float xz[2] = {static_cast<float>(ui.place_x), static_cast<float>(ui.place_z)};
    if (ImGui::DragFloat2("Place X / Z", xz, 2.0f)) ui.place_x = xz[0], ui.place_z = xz[1];
    if (!ui.tools.empty() && ImGui::Button("Place Item")) {
        const FlatHeightSampler ground(0.0);
        std::string err;
        if (auto id = wa.place_with_tool(ui.tools[ui.selected_tool], ui.place_x, ui.place_z, ground, &err)) {
            (void)ec.workspace.reload_world_preview(ec.app, &err);
            ec.workspace.push_console("placed entity #" + std::to_string(*id));
        }
    }
}

} // namespace

void draw_world_authoring_contents(EditorPanelContext& ec) {
    WorldAuthoringSession* wa = ec.state.docs.world_authoring.get();
    if (!wa || !wa->is_open()) {
        ImGui::TextDisabled("Open a .world map to inspect cells, layers, Level Instances, and placement.");
        return;
    }
    draw_layers_and_origin(ec, *wa);
    draw_cook_cells(*wa);
    draw_level_instances(ec, *wa, ec.state.world_ui);
    draw_placement_palette(ec, *wa, ec.state.world_ui);
}

} // namespace astral::editor
