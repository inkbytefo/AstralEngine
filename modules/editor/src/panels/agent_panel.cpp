#include "editor_state.hpp"
#include "theme/editor_theme.hpp"

#include <imgui.h>

#include <cstdio>
#include <string>

namespace astral::editor {

namespace {

void draw_provider_policy_popup(EditorPanelContext& ec, AgentTaskManager& mgr) {
    if (!ImGui::BeginPopup("##AgentSettings")) return;
    ImGui::TextDisabled("Agent Provider");
    ImGui::RadioButton("Built-in Offline (Deterministic)", &ec.state.docs.provider_index, 0);
    ImGui::RadioButton("Codex CLI (Local Process)", &ec.state.docs.provider_index, 1);
    ImGui::Separator();
    ImGui::TextDisabled("Safety Policy");
    AgentPolicy p = mgr.policy();
    bool changed = false;
    changed |= ImGui::Checkbox("Require approval for all edits", &p.require_approval_for_edits);
    changed |= ImGui::Checkbox("Allow external CLI commands", &p.allow_external_commands);
    changed |= ImGui::Checkbox("Allow network access", &p.allow_network);
    if (changed) mgr.set_policy(p);
    ImGui::EndPopup();
}

void draw_review_state(EditorPanelContext& ec, AgentTaskManager& mgr, AgentTaskRecord& task) {
    const EditorThemeTokens& t = editor_theme();
    const PreviewResult* pv = mgr.preview();
    draw_status_pill("Awaiting Review", t.accent_hover);
    if (task.proposal) ImGui::TextWrapped("%s", task.proposal->summary.c_str());

    // Genuine Before/After 3D Viewport switch
    const bool after = ec.state.viewport.preview_view == ViewportPreviewMode::CandidateAfter;
    if (ImGui::SmallButton(after ? "Viewing: After (3D Preview)" : "Viewing: Before (Live)")) {
        ec.state.viewport.preview_view = after ? ViewportPreviewMode::Live : ViewportPreviewMode::CandidateAfter;
    }

    if (task.proposal && !task.plan_steps.empty()) {
        ImGui::SeparatorText("Proposed Changes");
        for (std::size_t i = 0; i < task.plan_steps.size() && i < task.command_selection.size(); ++i) {
            bool checked = task.command_selection[i];
            if (ImGui::Checkbox(("##cmd" + std::to_string(i)).c_str(), &checked)) task.command_selection[i] = checked;
            ImGui::SameLine();
            ImGui::TextWrapped("%s", task.plan_steps[i].c_str());
        }
    }

    if (pv && !pv->diff.empty()) {
        ImGui::SeparatorText("Field Diff");
        if (ImGui::BeginTable("##DiffTbl", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, 110.0f))) {
            ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 68.0f);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
            for (const FieldChange& ch : pv->diff.changes) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", ch.kind.c_str());
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", ch.path.c_str());
            }
            ImGui::EndTable();
        }
    }

    // Human decision actions: Apply / Approve Selected / Revise / Discard
    ImGui::PushStyleColor(ImGuiCol_Button, t.accent);
    if (ImGui::Button("Apply")) {
        const TransactionResult res = mgr.approve_selected();
        if (res.ok) {
            ec.state.viewport.preview_view = ViewportPreviewMode::Live;
            ec.workspace.refresh_asset_list();
            sync_scene_to_world_live(ec);
            ec.state.ui.status_line = "Applied AI proposal";
        }
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
        mgr.discard("discarded by user");
        ec.state.viewport.preview_view = ViewportPreviewMode::Live;
    }

    char rbuf[256];
    std::snprintf(rbuf, sizeof rbuf, "%s", ec.state.ui.revision_input.c_str());
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 68.0f);
    if (ImGui::InputTextWithHint("##Revise", "Revise in plain language...", rbuf, sizeof rbuf)) {
        ec.state.ui.revision_input = rbuf;
    }
    ImGui::SameLine();
    if (ImGui::Button("Revise") && !ec.state.ui.revision_input.empty()) {
        std::string err;
        (void)mgr.request_revision(active_agent_provider(ec.state.docs), ec.state.ui.revision_input, true, &err);
        ec.state.ui.revision_input.clear();
    }
}

void draw_active_task(EditorPanelContext& ec, AgentTaskManager& mgr) {
    const EditorThemeTokens& t = editor_theme();
    AgentTaskRecord* task = mgr.current();
    if (!task || task->state == AgentTaskState::Idle) {
        ImGui::PushStyleColor(ImGuiCol_Text, t.text_muted);
        ImGui::TextWrapped("Describe what you want to build or change.");
        ImGui::Spacing();
        ImGui::TextWrapped("Edits are staged in an isolated preview before touching your scene.");
        ImGui::PopStyleColor();
        return;
    }
    switch (task->state) {
    case AgentTaskState::Planning:
    case AgentTaskState::Executing:
        draw_status_pill(task->state == AgentTaskState::Planning ? "Planning..." : "Working...", t.accent_hover);
        ImGui::TextWrapped("%s", task->prompt.c_str());
        for (const AgentTaskEvent& ev : task->events) ImGui::BulletText("%s: %s", ev.stage.c_str(), ev.message.c_str());
        if (ImGui::Button("Cancel")) mgr.cancel();
        break;
    case AgentTaskState::Review:
        draw_review_state(ec, mgr, *task);
        break;
    case AgentTaskState::Blocked:
        draw_status_pill("Action Needed", t.status_warn);
        ImGui::TextWrapped("%s", task->blocked_reason.c_str());
        if (ImGui::Button("Enable External CLI & Retry")) {
            AgentPolicy p = mgr.policy();
            p.allow_external_commands = true;
            mgr.set_policy(p);
            std::string err;
            (void)mgr.submit(active_agent_provider(ec.state.docs), task->context_entity, task->prompt, true, &err);
        }
        ImGui::SameLine();
        if (ImGui::Button("Dismiss")) mgr.clear_finished();
        break;
    case AgentTaskState::Failed:
        draw_status_pill("Failed", t.status_err);
        ImGui::TextWrapped("%s", task->error.c_str());
        if (ImGui::Button("Retry")) {
            std::string err;
            (void)mgr.submit(active_agent_provider(ec.state.docs), task->context_entity, task->prompt, true, &err);
        }
        ImGui::SameLine();
        if (ImGui::Button("Dismiss")) mgr.clear_finished();
        break;
    case AgentTaskState::Complete:
        draw_status_pill("Verified & Applied", t.status_ok);
        ImGui::Text("Applied %zu change(s).", task->applied_changes);
        if (ImGui::Button("Undo")) (void)ec.workspace.undo(ec.app);
        ImGui::SameLine();
        if (ImGui::Button("Done")) mgr.clear_finished();
        break;
    default: break;
    }
}

void draw_history_tab(const EditorPanelContext& ec, const AgentTaskManager& mgr) {
    ImGui::TextDisabled("Tasks (%zu)", mgr.history().size());
    for (auto it = mgr.history().rbegin(); it != mgr.history().rend(); ++it) {
        ImGui::BulletText("[%s] %s (%s)", std::string(agent_task_state_name(it->state)).c_str(),
                          it->prompt.c_str(), it->provider_id.c_str());
    }
    if (ec.state.docs.session) {
        ImGui::SeparatorText("Audit Log");
        const auto& entries = ec.state.docs.session->audit_log().entries();
        for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            ImGui::TextDisabled("#%llu [%s] %s: %s", static_cast<unsigned long long>(it->sequence),
                                it->action.c_str(), it->actor.c_str(), it->summary.c_str());
        }
    }
}

class AgentWorkspacePanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "agent"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "Astral Agent"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext& ec) const noexcept override { return ec.state.vis.agent; }

    void draw(EditorPanelContext& ec) override {
        if (!ImGui::Begin("Astral Agent", &ec.state.vis.agent)) {
            ImGui::End();
            return;
        }
        AgentTaskManager* mgr = ec.state.docs.tasks.get();
        if (!mgr) {
            ImGui::TextDisabled("Agent service unavailable (manual mode active).");
            ImGui::End();
            return;
        }

        // Header tabs: Task | History + Provider/Policy settings button
        if (ImGui::SmallButton(ec.state.ui.agent_tab == 0 ? "[Task]" : "Task")) ec.state.ui.agent_tab = 0;
        ImGui::SameLine();
        if (ImGui::SmallButton(ec.state.ui.agent_tab == 1 ? "[History]" : "History")) ec.state.ui.agent_tab = 1;
        ImGui::SameLine(ImGui::GetWindowWidth() - 78.0f);
        if (ImGui::SmallButton("Policy")) ImGui::OpenPopup("##AgentSettings");
        draw_provider_policy_popup(ec, *mgr);
        ImGui::Separator();

        if (ec.state.ui.agent_tab == 1) {
            draw_history_tab(ec, *mgr);
            ImGui::End();
            return;
        }

        // Visible, revocable selection context pill
        if (ec.state.selection.primary.valid() && ec.state.selection.attach_to_agent) {
            ImGui::TextDisabled("Context: Entity #%s", ec.state.selection.primary.to_string().c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("x##DetCtx")) ec.state.selection.attach_to_agent = false;
        } else if (ec.state.selection.primary.valid()) {
            if (ImGui::SmallButton("+ Attach selected entity")) ec.state.selection.attach_to_agent = true;
        }

        // Compact intent input
        char buf[512];
        std::snprintf(buf, sizeof buf, "%s", ec.state.ui.prompt_input.c_str());
        if (ImGui::InputTextMultiline("##Intent", buf, sizeof buf, ImVec2(-1.0f, 64.0f))) {
            ec.state.ui.prompt_input = buf;
        }
        const bool submit_shortcut =
            ImGui::IsItemFocused() && (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) &&
            ImGui::IsKeyPressed(ImGuiKey_Enter, false);
        const bool can_submit = !mgr->busy() && !ec.state.ui.prompt_input.empty();
        ImGui::BeginDisabled(!can_submit);
        if (ImGui::Button("Propose") || (can_submit && submit_shortcut)) {
            const DocumentEntityId ctx_id =
                ec.state.selection.attach_to_agent ? ec.state.selection.primary : DocumentEntityId{};
            std::string err;
            (void)mgr->submit(active_agent_provider(ec.state.docs), ctx_id, ec.state.ui.prompt_input, true, &err);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", std::string(active_agent_provider(ec.state.docs).display_name()).c_str());

        ImGui::Separator();
        draw_active_task(ec, *mgr);
        ImGui::End();
    }
};

} // namespace

std::unique_ptr<EditorPanel> make_agent_panel() { return std::make_unique<AgentWorkspacePanel>(); }

} // namespace astral::editor
