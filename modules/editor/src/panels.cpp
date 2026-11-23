#include <astral/editor/workspace.hpp>

#include <imgui.h>

#include <cstdio>
#include <string>
#include <vector>

namespace astral::editor {

namespace {

bool edit_scalar_field(const FieldInfo& f, Json& val) {
    if (f.kind == FieldKind::Bool) {
        bool b = val.as_bool();
        if (ImGui::Checkbox(f.name.c_str(), &b)) return val = b, true;
    } else if (f.kind == FieldKind::Float || f.kind == FieldKind::Int || f.kind == FieldKind::UInt) {
        float x = static_cast<float>(val.as_number());
        if (ImGui::DragFloat(f.name.c_str(), &x, 0.05f)) return val = static_cast<double>(x), true;
    } else if (f.kind == FieldKind::String) {
        char buf[256];
        std::snprintf(buf, sizeof buf, "%s", val.as_string().c_str());
        if (ImGui::InputText(f.name.c_str(), buf, sizeof buf, ImGuiInputTextFlags_EnterReturnsTrue)) {
            val = std::string(buf);
            return true;
        }
    } else if (f.kind == FieldKind::Vec3 && val.is_array() && val.size() == 3) {
        float v[3] = {static_cast<float>(val[0].as_number()), static_cast<float>(val[1].as_number()),
                      static_cast<float>(val[2].as_number())};
        if (ImGui::DragFloat3(f.name.c_str(), v, 0.05f)) {
            Json arr = Json::array();
            for (float c : v) arr.push_back(static_cast<double>(c));
            val = std::move(arr);
            return true;
        }
    }
    return false;
}

void draw_menu_bar(EditorWorkspace& ws, Context& ctx) {
    if (!ImGui::BeginMainMenuBar()) return;
    ImGui::TextDisabled("Astral Editor");
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Reload Scene", "Ctrl+R", false, ws.session() && ws.session()->is_open())) {
            std::string err;
            (void)ws.reload_live_scene(ctx, &err);
        }
        if (ImGui::MenuItem("Quit")) ctx.quit();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        const bool can_u = ws.session() && ws.session()->can_undo();
        const bool can_r = ws.session() && ws.session()->can_redo();
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, can_u)) (void)ws.undo(ctx);
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, can_r)) (void)ws.redo(ctx);
        ImGui::EndMenu();
    }
    if (ws.session() && ws.session()->is_open()) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 260);
        ImGui::TextDisabled("rev %llu | %.1f ms",
                            static_cast<unsigned long long>(ws.session()->document().revision.number),
                            ctx.profiler ? static_cast<double>(ctx.profiler->last_frame_ms()) : 0.0);
    }
    ImGui::EndMainMenuBar();
}

void draw_asset_browser(EditorWorkspace& ws, Context& ctx, const std::filesystem::path& asset_root) {
    if (!ImGui::Begin("Assets")) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Refresh")) ws.refresh_asset_list();
    ImGui::Separator();
    for (const std::filesystem::path& p : ws.asset_files()) {
        std::error_code ec;
        const std::filesystem::path rel = std::filesystem::relative(p, asset_root, ec);
        const std::string label = (!ec && !rel.empty()) ? rel.generic_string() : p.filename().string();
        if (ImGui::Selectable(label.c_str())) {
            std::string err;
            (void)ws.open_document(ctx, p, &err);
        }
    }
    ImGui::End();
}

void draw_hierarchy(EditorWorkspace& ws) {
    if (!ImGui::Begin("Hierarchy")) {
        ImGui::End();
        return;
    }
    const EditorSession* s = ws.session();
    if (!s || !s->is_open()) {
        ImGui::TextDisabled("No document open");
        ImGui::End();
        return;
    }
    const Json& doc = (ws.preview_mode() && ws.review() && ws.review()->preview())
                          ? ws.review()->preview()->candidate_doc.root
                          : s->document().root;
    if (const Json* ents = doc.find("entities"); ents && ents->is_array()) {
        for (std::size_t i = 0; i < ents->size(); ++i) {
            const Json& rec = (*ents)[i];
            const auto id = static_cast<std::uint32_t>(rec["id"].as_number(0.0));
            std::string label = "#" + std::to_string(id);
            if (const Json* c = rec.find("components"); c && c->find("astral::Name")) {
                label += " " + (*c)["astral::Name"]["value"].as_string();
            } else if (const Json* pf = rec.find("prefab")) {
                label += " [" + pf->as_string() + "]";
            }
            const DocumentEntityId doc_id{id, {}};
            if (ImGui::Selectable(label.c_str(), ws.selected_entity() == doc_id)) ws.select_entity(doc_id);
        }
    }
    ImGui::End();
}

void draw_inspector(EditorWorkspace& ws, Context& ctx) {
    if (!ImGui::Begin("Inspector")) {
        ImGui::End();
        return;
    }
    EditorSession* s = ws.session();
    if (!s || !s->is_open() || !ws.selected_entity().valid()) {
        ImGui::TextDisabled("Select an entity in Hierarchy");
        ImGui::End();
        return;
    }
    const Json* rec = s->document().find_record(ws.selected_entity().record_id);
    if (!rec) {
        ImGui::TextDisabled("Record not found");
        ImGui::End();
        return;
    }
    const Json* comps = rec->find("components");
    if (comps && comps->is_object()) {
        for (const auto& [comp_name, comp_json] : comps->as_object()) {
            const TypeRegistry::Entry* entry = ctx.types.find(comp_name);
            if (!entry || !ImGui::CollapsingHeader(comp_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) continue;
            ImGui::PushID(comp_name.c_str());
            for (const FieldInfo& f : entry->fields) {
                const Json* cur = comp_json.find(f.name);
                if (!cur) continue;
                Json edited = *cur;
                if (edit_scalar_field(f, edited)) {
                    Transaction tx;
                    tx.summary = "Edit " + comp_name + "." + f.name;
                    tx.expected_revision = s->document().revision;
                    tx.commands.push_back({.kind = CommandKind::SetComponentField, .target = ws.selected_entity(),
                                           .component = comp_name, .field = f.name, .value = std::move(edited)});
                    if (s->apply(tx).ok) {
                        std::string err;
                        (void)ws.reload_live_scene(ctx, &err);
                    }
                }
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
}

void draw_ai_panel(EditorWorkspace& ws, Context& ctx, std::string& prompt_buf, bool& preview_mode) {
    if (!ImGui::Begin("AI Assistant & Review")) {
        ImGui::End();
        return;
    }
    char buf[256];
    std::snprintf(buf, sizeof buf, "%s", prompt_buf.c_str());
    if (ImGui::InputText("Prompt", buf, sizeof buf)) prompt_buf = buf;
    if (ImGui::Button("Request Proposal")) {
        std::string err;
        (void)ws.request_ai_proposal(prompt_buf, &err);
    }
    if (ws.review() && ws.review()->has_active()) {
        const AgentProposal* p = ws.review()->active();
        const PreviewResult* pv = ws.review()->preview();
        ImGui::Separator();
        ImGui::Text("Proposal %s (%s)", p->proposal_id.c_str(), p->agent_id.c_str());
        ImGui::TextWrapped("%s", p->summary.c_str());
        ImGui::Checkbox("Preview in Hierarchy", &preview_mode);
        if (pv) {
            ImGui::TextDisabled("Isolated ECS preview: %zu entities | %s", pv->entity_count, pv->diff.summary().c_str());
            for (const FieldChange& ch : pv->diff.changes) {
                ImGui::BulletText("[%s] %s", ch.kind.c_str(), ch.path.c_str());
            }
        }
        if (ImGui::Button("Approve & Save")) (void)ws.approve_proposal(ctx);
        ImGui::SameLine();
        if (ImGui::Button("Reject")) ws.reject_proposal();
    }
    if (ws.session()) {
        ImGui::Separator();
        ImGui::TextDisabled("Audit log (%zu entries)", ws.session()->audit_log().entries().size());
        for (const AuditEntry& e : ws.session()->audit_log().entries()) {
            ImGui::TextDisabled("#%llu [%s] %s: %s", static_cast<unsigned long long>(e.sequence),
                                e.action.c_str(), e.actor.c_str(), e.summary.c_str());
        }
    }
    ImGui::End();
}

void draw_console_and_profiler(const EditorWorkspace& ws, Context& ctx) {
    if (ImGui::Begin("Console & Profiler")) {
        if (ctx.profiler) {
            ImGui::Text("Frame: %.2f ms", static_cast<double>(ctx.profiler->last_frame_ms()));
            ImGui::Separator();
        }
        for (const std::string& line : ws.console_lines()) ImGui::TextUnformatted(line.c_str());
    }
    ImGui::End();
}

} // namespace

void EditorWorkspace::draw_panels(Context& ctx) {
#ifdef ImGuiConfigFlags_DockingEnable
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
#endif
    draw_menu_bar(*this, ctx);
    draw_asset_browser(*this, ctx, desc_.asset_root);
    draw_hierarchy(*this);
    draw_inspector(*this, ctx);
    draw_world_panel(ctx);
    draw_ai_panel(*this, ctx, prompt_input_, preview_mode_);
    draw_console_and_profiler(*this, ctx);
}

} // namespace astral::editor
