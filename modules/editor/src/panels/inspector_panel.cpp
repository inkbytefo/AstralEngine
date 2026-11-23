#include "editor_state.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace astral::editor {

namespace {

bool contains_ci(std::string_view hay, std::string_view needle) {
    return hay.find(needle) != std::string_view::npos;
}

bool edit_nested_json(const char* label, Json& j, bool& active_drag) {
    bool changed = false;
    ImGui::PushID(label);
    if (j.is_bool()) {
        bool b = j.as_bool();
        if (ImGui::Checkbox(label, &b)) j = b, changed = true;
    } else if (j.is_number()) {
        float x = static_cast<float>(j.as_number());
        if (ImGui::DragFloat(label, &x, 0.02f)) j = static_cast<double>(x), changed = true;
        active_drag |= ImGui::IsItemActive();
    } else if (j.is_string()) {
        char buf[256];
        std::snprintf(buf, sizeof buf, "%s", j.as_string().c_str());
        if (ImGui::InputText(label, buf, sizeof buf, ImGuiInputTextFlags_EnterReturnsTrue)) {
            j = std::string(buf);
            changed = true;
        }
    } else if (j.is_array() && j.size() >= 2 && j.size() <= 4) {
        float v[4] = {};
        for (std::size_t i = 0; i < j.size(); ++i) v[i] = static_cast<float>(j[i].as_number());
        const bool is_color = contains_ci(label, "color") || contains_ci(label, "emissive");
        if (is_color && j.size() == 3) changed = ImGui::ColorEdit3(label, v, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
        else if (is_color && j.size() == 4) changed = ImGui::ColorEdit4(label, v, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
        else if (j.size() == 2) changed = ImGui::DragFloat2(label, v, 0.02f);
        else if (j.size() == 3) changed = ImGui::DragFloat3(label, v, 0.02f);
        else changed = ImGui::DragFloat4(label, v, 0.02f);
        active_drag |= ImGui::IsItemActive();
        if (changed) {
            Json out = Json::array();
            for (std::size_t i = 0; i < j.size(); ++i) out.push_back(static_cast<double>(v[i]));
            j = std::move(out);
        }
    } else if (j.is_object() && ImGui::TreeNode(label)) {
        const std::vector<std::pair<std::string, Json>> items(j.as_object().begin(), j.as_object().end());
        for (const auto& [k, v] : items) {
            Json copy = v;
            if (edit_nested_json(k.c_str(), copy, active_drag)) {
                j[k] = std::move(copy);
                changed = true;
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
    return changed;
}

bool edit_reflected_field(const FieldInfo& f, Json& val, bool& active_drag) {
    if (f.kind == FieldKind::Enum && !f.enumerators.empty()) {
        const std::string cur = val.as_string();
        bool changed = false;
        if (ImGui::BeginCombo(f.name.c_str(), cur.c_str())) {
            for (const auto& [ename, _] : f.enumerators) {
                if (ImGui::Selectable(ename.c_str(), cur == ename)) {
                    val = ename;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }
    return edit_nested_json(f.name.c_str(), val, active_drag);
}

void draw_add_component_combo(EditorPanelContext& ec, EditorSession& s, DocumentEntityId target, const Json* comps) {
    if (!ImGui::BeginCombo("##AddComp", "+ Add Component")) return;
    for (const TypeRegistry::Entry& entry : ec.app.types.entries()) {
        if (comps && comps->find(entry.name)) continue;
        if (ImGui::Selectable(std::string(entry.name).c_str())) {
            Transaction tx;
            tx.summary = "Add " + std::string(entry.name);
            tx.expected_revision = s.document().revision;
            tx.commands.push_back({.kind = CommandKind::AddComponent, .target = target,
                                   .component = std::string(entry.name), .value = Json::object()});
            if (s.apply(tx).ok) sync_scene_to_world_live(ec);
        }
    }
    ImGui::EndCombo();
}

class InspectorPanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "inspector"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "Inspector"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext& ec) const noexcept override { return ec.state.vis.inspector; }

    void draw(EditorPanelContext& ec) override {
        if (!ImGui::Begin("Inspector", &ec.state.vis.inspector)) {
            ImGui::End();
            return;
        }
        EditorSession* s = ec.state.docs.session.get();
        const DocumentEntityId target = ec.state.selection.primary;
        if (!s || !s->is_open() || !target.valid()) {
            ImGui::TextDisabled("Select an entity in Scene or Viewport.");
            ImGui::End();
            return;
        }
        const Json* rec = s->document().find_record(target.record_id);
        if (!rec) {
            ImGui::TextDisabled("Entity record #%u not found.", target.record_id);
            ImGui::End();
            return;
        }
        ImGui::Text("Entity #%s", target.to_string().c_str());
        if (const Json* pf = rec->find("prefab"); pf && !pf->as_string().empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("Prefab: %s", pf->as_string().c_str());
        }
        ImGui::Separator();

        const Json* comps = rec->find("components");
        std::string comp_to_remove;
        if (comps && comps->is_object()) {
            const std::vector<std::pair<std::string, Json>> list(comps->as_object().begin(), comps->as_object().end());
            for (const auto& [comp_name, comp_json] : list) {
                const TypeRegistry::Entry* entry = ec.app.types.find(comp_name);
                if (!entry) continue;
                ImGui::PushID(comp_name.c_str());
                const bool open = ImGui::CollapsingHeader(comp_name.c_str(),
                                                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
                ImGui::SameLine(ImGui::GetWindowWidth() - 34.0f);
                if (ImGui::SmallButton("x")) comp_to_remove = comp_name;
                if (open) {
                    for (const FieldInfo& f : entry->fields) {
                        const Json* cur = comp_json.find(f.name);
                        if (!cur) continue;
                        Json edited = *cur;
                        bool dragging = false;
                        if (edit_reflected_field(f, edited, dragging)) {
                            Transaction tx;
                            tx.summary = "Edit " + comp_name + "." + f.name;
                            tx.expected_revision = s->document().revision;
                            if (dragging) tx.coalesce_key = "insp:" + target.to_string() + ":" + comp_name + "." + f.name;
                            tx.commands.push_back({.kind = CommandKind::SetComponentField, .target = target,
                                                   .component = comp_name, .field = f.name, .value = std::move(edited)});
                            if (s->apply(tx).ok) sync_scene_to_world_live(ec);
                        }
                    }
                }
                ImGui::PopID();
            }
        }
        if (!comp_to_remove.empty()) {
            Transaction tx;
            tx.summary = "Remove " + comp_to_remove;
            tx.expected_revision = s->document().revision;
            tx.commands.push_back({.kind = CommandKind::RemoveComponent, .target = target, .component = comp_to_remove});
            if (s->apply(tx).ok) sync_scene_to_world_live(ec);
        }
        ImGui::Separator();
        draw_add_component_combo(ec, *s, target, comps);
        ImGui::End();
    }
};

} // namespace

std::unique_ptr<EditorPanel> make_inspector_panel() { return std::make_unique<InspectorPanel>(); }

} // namespace astral::editor
