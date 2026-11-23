#include <astral/editor/workspace.hpp>

#include <imgui.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace astral::editor {

namespace {

void draw_layers_and_origin(EditorWorkspace& ws, Context& ctx, WorldAuthoringSession& wa) {
    ImGui::Text("Map: %s (%zu source .ent)", wa.world_path().filename().string().c_str(), wa.raw_entities().size());
    if (ImGui::Button("Reload Preview")) {
        std::string err;
        (void)ws.reload_world_preview(ctx, &err);
    }

    WorldPosition origin = wa.preview_origin();
    float orig3[3] = {static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z)};
    if (ImGui::DragFloat3("Floating Origin", orig3, 16.0f)) {
        wa.set_preview_origin({orig3[0], orig3[1], orig3[2]});
        std::string err;
        (void)ws.reload_world_preview(ctx, &err);
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
            (void)ws.reload_world_preview(ctx, &err);
        }
    }
}

void draw_cook_cells(const WorldAuthoringSession& wa) {
    if (!ImGui::CollapsingHeader("Cook Cells & Streaming Ownership", ImGuiTreeNodeFlags_DefaultOpen)) return;
    std::vector<std::string> errs;
    const std::vector<CellOwnershipInfo> cells = wa.inspect_cells(&errs);
    ImGui::TextDisabled("%zu active cells | %zu warnings", cells.size(), errs.size());
    for (const CellOwnershipInfo& c : cells) {
        ImGui::BulletText("%s [grid=%u (%d,%d)] pos=(%.0f, %.0f) size=%.0fm (%zu ents)", c.name.c_str(),
                          static_cast<unsigned>(c.key.grid), c.key.x, c.key.z, c.min_x, c.min_z, c.size, c.entity_count);
    }
}

void draw_level_instances(EditorWorkspace& ws, Context& ctx, WorldAuthoringSession& wa, WorldAuthoringUiState& ui) {
    if (!ImGui::CollapsingHeader("Level Instances (Override vs Edit-in-Context)", ImGuiTreeNodeFlags_DefaultOpen)) return;
    std::string selected_scene;
    for (const MapEntity& me : wa.raw_entities()) {
        const auto parsed = parse_level_instance_entity(make_entity_file(me));
        if (!parsed) continue;
        char label[192];
        std::snprintf(label, sizeof label, "#%llu %s (%.0f, %.0f, %.0f) [%zu overrides]",
                      static_cast<unsigned long long>(parsed->instance_id), parsed->scene_path.c_str(),
                      parsed->position[0], parsed->position[1], parsed->position[2],
                      parsed->overrides.is_object() ? parsed->overrides.as_object().size() : std::size_t{0});
        const bool is_sel = ui.selected_instance_id == parsed->instance_id;
        if (ImGui::Selectable(label, is_sel)) ui.selected_instance_id = parsed->instance_id;
        if (ui.selected_instance_id == parsed->instance_id) selected_scene = parsed->scene_path;
    }

    if (ui.selected_instance_id == 0 || selected_scene.empty()) {
        ImGui::TextDisabled("Select a Level Instance above to apply an Instance Override or Edit-in-Context.");
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
    edit_str("String Value", ui.edit_value);

    if (ImGui::Button("Apply Instance Override (.ent)")) {
        std::string err;
        if (wa.edit_instance_override(ui.selected_instance_id, ui.edit_local_path, ui.edit_component, ui.edit_field,
                                      Json(ui.edit_value), &err)) {
            (void)ws.reload_world_preview(ctx, &err);
            ws.push_console("applied instance override on #" + std::to_string(ui.selected_instance_id));
        } else {
            ws.push_console("override error: " + err);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Edit-in-Context (Source .scene)")) {
        const auto rec_id = static_cast<std::uint32_t>(std::strtoul(ui.edit_local_path.c_str(), nullptr, 10));
        std::string err;
        if (wa.edit_source_scene_in_context(selected_scene, rec_id, ui.edit_component, ui.edit_field,
                                            Json(ui.edit_value), &err)) {
            (void)ws.reload_world_preview(ctx, &err);
            ws.push_console("edited source scene in-context: " + selected_scene);
        } else {
            ws.push_console("edit-in-context error: " + err);
        }
    }
}

void draw_placement_palette(EditorWorkspace& ws, Context& ctx, WorldAuthoringSession& wa, WorldAuthoringUiState& ui) {
    if (!ImGui::CollapsingHeader("Placement Palette", ImGuiTreeNodeFlags_DefaultOpen)) return;
    for (std::size_t i = 0; i < ui.tools.size(); ++i) {
        const PlacementToolDef& tool = ui.tools[i];
        std::string label = "[" + std::string(placement_category_name(tool.category)) + "] " + tool.label;
        if (ImGui::Selectable(label.c_str(), ui.selected_tool == i)) ui.selected_tool = i;
    }
    float xz[2] = {static_cast<float>(ui.place_x), static_cast<float>(ui.place_z)};
    if (ImGui::DragFloat2("Place X / Z (m)", xz, 2.0f)) {
        ui.place_x = xz[0];
        ui.place_z = xz[1];
    }
    if (!ui.tools.empty() && ImGui::Button("Place Item")) {
        const FlatHeightSampler ground(0.0);
        std::string err;
        if (auto id = wa.place_with_tool(ui.tools[ui.selected_tool], ui.place_x, ui.place_z, ground, &err)) {
            (void)ws.reload_world_preview(ctx, &err);
            ws.push_console("placed entity #" + std::to_string(*id));
        } else {
            ws.push_console("place error: " + err);
        }
    }
}

} // namespace

void EditorWorkspace::draw_world_panel(Context& ctx) {
    if (!ImGui::Begin("World Authoring")) {
        ImGui::End();
        return;
    }
    if (!world_authoring_ || !world_authoring_->is_open()) {
        ImGui::TextDisabled("Open a .world map in Assets to edit cells, layers, Level Instances, and placement.");
        ImGui::End();
        return;
    }
    draw_layers_and_origin(*this, ctx, *world_authoring_);
    draw_cook_cells(*world_authoring_);
    draw_level_instances(*this, ctx, *world_authoring_, world_ui_);
    draw_placement_palette(*this, ctx, *world_authoring_, world_ui_);
    ImGui::End();
}

} // namespace astral::editor
