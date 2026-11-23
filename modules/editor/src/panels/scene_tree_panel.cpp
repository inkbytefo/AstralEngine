#include "editor_state.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace astral::editor {

namespace {

bool matches_filter(std::string_view text, std::string_view filter) {
    if (filter.empty()) return true;
    auto it = std::search(text.begin(), text.end(), filter.begin(), filter.end(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    });
    return it != text.end();
}

struct TreeRow {
    std::uint32_t id = 0;
    std::string label;
    bool is_prefab = false;
};

std::vector<TreeRow> collect_rows(const Json& doc, std::string_view filter) {
    std::vector<TreeRow> rows;
    const Json* ents = doc.find("entities");
    if (!ents || !ents->is_array()) return rows;
    rows.reserve(ents->size());
    for (std::size_t i = 0; i < ents->size(); ++i) {
        const Json& rec = (*ents)[i];
        const auto id = static_cast<std::uint32_t>(rec["id"].as_number(0.0));
        std::string label;
        bool is_prefab = false;
        if (const Json* c = rec.find("components"); c && c->find("astral::Name")) {
            label = (*c)["astral::Name"]["value"].as_string();
        }
        if (const Json* pf = rec.find("prefab"); pf && !pf->as_string().empty()) {
            is_prefab = true;
            if (label.empty()) label = pf->as_string();
        }
        if (label.empty()) label = "Entity #" + std::to_string(id);
        if (matches_filter(label, filter) || matches_filter(std::to_string(id), filter)) {
            rows.push_back({id, std::move(label), is_prefab});
        }
    }
    return rows;
}

void create_default_entity(EditorPanelContext& ec) {
    EditorSession* s = ec.state.docs.session.get();
    if (!s || !s->is_open()) return;
    Json comps = Json::object();
    Json nm = Json::object();
    nm["value"] = "New Cube";
    comps["astral::Name"] = std::move(nm);
    Json tf = Json::object();
    Json pos = Json::array();
    pos.push_back(0.0);
    pos.push_back(0.5);
    pos.push_back(0.0);
    tf["position"] = std::move(pos);
    comps["astral::Transform"] = std::move(tf);
    Json mesh = Json::object();
    mesh["shape"] = "Cube";
    comps["astral::PrimitiveMesh"] = std::move(mesh);

    const std::uint32_t new_id = s->document().next_record_id();
    Transaction tx;
    tx.summary = "Create entity #" + std::to_string(new_id);
    tx.expected_revision = s->document().revision;
    tx.commands.push_back({.kind = CommandKind::CreateEntity, .target = {new_id, {}}, .value = std::move(comps)});
    if (s->apply(tx).ok) {
        ec.workspace.select_entity({new_id, {}});
        sync_scene_to_world_live(ec);
    }
}

void draw_entity_context_menu(EditorPanelContext& ec, DocumentEntityId doc_id, const std::string& label) {
    if (!ImGui::BeginPopupContextItem()) return;
    if (ImGui::MenuItem("Inspect", "I")) {
        ec.workspace.select_entity(doc_id);
        ec.state.vis.inspector = true;
    }
    if (ImGui::MenuItem("Focus in Viewport", "F")) {
        ec.workspace.select_entity(doc_id);
        ec.workspace.focus_selected(ec.app);
    }
    if (ImGui::MenuItem("Rename")) {
        ec.state.ui.renaming_entity = doc_id;
        ec.state.ui.rename_buffer = label;
    }
    if (ImGui::MenuItem("Delete", "Del") && ec.state.docs.session && ec.state.docs.session->is_open()) {
        Transaction tx;
        tx.summary = "Delete #" + doc_id.to_string();
        tx.expected_revision = ec.state.docs.session->document().revision;
        tx.commands.push_back({.kind = CommandKind::DeleteEntity, .target = doc_id});
        if (ec.state.docs.session->apply(tx).ok) {
            if (ec.state.selection.primary == doc_id) ec.workspace.select_entity({});
            sync_scene_to_world_live(ec);
        }
    }
    ImGui::EndPopup();
}

class SceneTreePanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "scene_tree"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "Scene"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext& ec) const noexcept override { return ec.state.vis.scene_tree; }

    void draw(EditorPanelContext& ec) override {
        if (!ImGui::Begin("Scene", &ec.state.vis.scene_tree)) {
            ImGui::End();
            return;
        }
        const EditorSession* s = ec.state.docs.session.get();
        if (!s || !s->is_open()) {
            ImGui::TextDisabled("No scene open.");
            ImGui::End();
            return;
        }

        // Compact search bar + Create Entity button
        char buf[128];
        std::snprintf(buf, sizeof buf, "%s", ec.state.selection.tree_filter.c_str());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 32.0f);
        if (ImGui::InputTextWithHint("##TreeFilter", "Search scene...", buf, sizeof buf)) {
            ec.state.selection.tree_filter = buf;
        }
        ImGui::SameLine();
        if (ImGui::Button("+") && ec.workspace.play_state() == PlayState::Stopped) create_default_entity(ec);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add Entity");
        ImGui::Separator();

        const PreviewResult* pv = (ec.state.viewport.preview_view == ViewportPreviewMode::CandidateAfter &&
                                   ec.state.docs.tasks)
                                      ? ec.state.docs.tasks->preview()
                                      : nullptr;
        const Json& doc = pv ? pv->candidate_doc.root : s->document().root;
        const std::vector<TreeRow> rows = collect_rows(doc, ec.state.selection.tree_filter);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rows.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const TreeRow& row = rows[static_cast<std::size_t>(i)];
                const DocumentEntityId doc_id{row.id, {}};
                ImGui::PushID(static_cast<int>(row.id));

                if (ec.state.ui.renaming_entity == doc_id) {
                    char rbuf[128];
                    std::snprintf(rbuf, sizeof rbuf, "%s", ec.state.ui.rename_buffer.c_str());
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::InputText("##rename", rbuf, sizeof rbuf, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        Transaction tx;
                        tx.summary = "Rename #" + doc_id.to_string();
                        tx.expected_revision = s->document().revision;
                        tx.commands.push_back({.kind = CommandKind::SetComponentField, .target = doc_id,
                                               .component = "astral::Name", .field = "value", .value = std::string(rbuf)});
                        if (ec.state.docs.session->apply(tx).ok) sync_scene_to_world_live(ec);
                        ec.state.ui.renaming_entity = {};
                    }
                    ImGui::PopID();
                    continue;
                }

                const bool is_selected = std::find(ec.state.selection.multi.begin(), ec.state.selection.multi.end(),
                                                   doc_id) != ec.state.selection.multi.end();
                std::string disp = row.label + (row.is_prefab ? "  [P]" : "");
                if (ImGui::Selectable(disp.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (ImGui::GetIO().KeyCtrl) {
                        if (is_selected) std::erase(ec.state.selection.multi, doc_id);
                        else ec.state.selection.multi.push_back(doc_id);
                        ec.state.selection.primary = doc_id;
                    } else {
                        ec.workspace.select_entity(doc_id);
                    }
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ec.state.vis.inspector = true;
                }
                draw_entity_context_menu(ec, doc_id, row.label);
                ImGui::PopID();
            }
        }
        ImGui::End();
    }
};

} // namespace

std::unique_ptr<EditorPanel> make_scene_tree_panel() { return std::make_unique<SceneTreePanel>(); }

} // namespace astral::editor
