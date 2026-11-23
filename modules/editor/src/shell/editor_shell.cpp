#include "editor_state.hpp"
#include "theme/editor_theme.hpp"

#include <imgui.h>
#ifdef IMGUI_HAS_DOCK
#include <imgui_internal.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace astral::editor {

namespace {

bool icontains(std::string_view hay, std::string_view needle) {
    if (needle.empty()) return true;
    auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    });
    return it != hay.end();
}

void handle_global_shortcuts(EditorPanelContext& ec) {
    const ImGuiIO& io = ImGui::GetIO();
    const bool ctrl = io.KeyCtrl || io.KeySuper;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        ec.state.vis.command_palette = !ec.state.vis.command_palette;
        ec.state.ui.palette_query.clear();
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        (void)ec.workspace.save_document();
        return;
    }
    if (ctrl && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        (void)ec.workspace.undo(ec.app);
        return;
    }
    if (ctrl && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        (void)ec.workspace.redo(ec.app);
        return;
    }
    if (io.WantTextInput) return;

    const bool rmb = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (!rmb && ec.state.viewport.focused) {
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) ec.workspace.set_gizmo_mode(GizmoMode::Select);
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) ec.workspace.set_gizmo_mode(GizmoMode::Translate);
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) ec.workspace.set_gizmo_mode(GizmoMode::Rotate);
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) ec.workspace.set_gizmo_mode(GizmoMode::Scale);
        if (ImGui::IsKeyPressed(ImGuiKey_F, false)) ec.workspace.focus_selected(ec.app);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_I, false) && ec.state.selection.primary.valid()) {
        ec.state.vis.inspector = !ec.state.vis.inspector;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && ec.state.selection.primary.valid() &&
        ec.state.docs.session && ec.state.docs.session->is_open()) {
        Transaction tx;
        tx.summary = "Delete #" + ec.state.selection.primary.to_string();
        tx.expected_revision = ec.state.docs.session->document().revision;
        tx.commands.push_back({.kind = CommandKind::DeleteEntity, .target = ec.state.selection.primary});
        if (ec.state.docs.session->apply(tx).ok) {
            ec.workspace.select_entity({});
            sync_scene_to_world_live(ec);
        }
    }
}

void draw_workspace_menu_popup(EditorPanelContext& ec) {
    if (!ImGui::BeginPopup("##WorkspaceMenu")) return;
    if (ImGui::MenuItem("Focus Workspace", nullptr, ec.state.vis.preset == WorkspacePreset::Focus)) {
        ec.workspace.set_workspace_preset(WorkspacePreset::Focus);
    }
    if (ImGui::MenuItem("Advanced Workspace", nullptr, ec.state.vis.preset == WorkspacePreset::Advanced)) {
        ec.workspace.set_workspace_preset(WorkspacePreset::Advanced);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Workspace Layout")) ec.workspace.reset_workspace_layout();
    if (ImGui::MenuItem("Command Palette", "Ctrl+K")) ec.state.vis.command_palette = true;
    if (ImGui::MenuItem("Keyboard Shortcuts")) ec.state.vis.shortcuts_modal = true;
    ImGui::Separator();
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, ec.state.docs.session && ec.state.docs.session->can_undo())) {
        (void)ec.workspace.undo(ec.app);
    }
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, ec.state.docs.session && ec.state.docs.session->can_redo())) {
        (void)ec.workspace.redo(ec.app);
    }
    if (ImGui::MenuItem("Quit")) ec.app.quit();
    ImGui::EndPopup();
}

void draw_doc_switcher(EditorPanelContext& ec) {
    std::string doc_label = ec.state.desc.project_name + " / ";
    if (ec.state.docs.world_mode && ec.state.docs.world_authoring && ec.state.docs.world_authoring->is_open()) {
        doc_label += ec.state.docs.world_authoring->world_path().filename().string();
    } else if (ec.state.docs.session && ec.state.docs.session->is_open()) {
        doc_label += ec.state.docs.session->document().path.filename().string();
    } else {
        doc_label += "No Document";
    }
    if (ImGui::Button((doc_label + "  v").c_str())) ImGui::OpenPopup("##DocSwitcher");
    if (!ImGui::BeginPopup("##DocSwitcher")) return;
    ImGui::TextDisabled("Open Scene / World");
    ImGui::Separator();
    for (const auto& p : ec.state.ui.asset_files) {
        const std::string ext = p.extension().string();
        if (ext != ".scene" && ext != ".world" && ext != ".prefab") continue;
        std::error_code ec_rel;
        const auto rel = std::filesystem::relative(p, ec.state.desc.asset_root, ec_rel);
        const std::string item = (!ec_rel && !rel.empty()) ? rel.generic_string() : p.filename().string();
        if (ImGui::MenuItem(item.c_str())) {
            std::string err;
            (void)ec.workspace.open_document(ec.app, p, &err);
        }
    }
    ImGui::EndPopup();
}

void draw_top_toolbar(EditorPanelContext& ec, float width) {
    const EditorThemeTokens& t = editor_theme();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoDocking;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(width, t.toolbar_height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, t.bg_base);
    if (ImGui::Begin("##TopToolbar", nullptr, flags)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(t.accent_hover, "ASTRAL");
        ImGui::SameLine(0.0f, 14.0f);
        draw_doc_switcher(ec);

        const float center_x = std::max(width * 0.5f - 82.0f, ImGui::GetCursorPosX() + 12.0f);
        ImGui::SameLine(center_x);
        const PlayState ps = ec.workspace.play_state();
        if (ps == PlayState::Stopped) {
            if (ImGui::Button("Play")) {
                std::string err;
                (void)ec.workspace.enter_play_mode(ec.app, &err);
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, t.accent);
            if (ImGui::Button("Stop")) ec.workspace.stop_play_mode(ec.app);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::Button(ps == PlayState::Paused ? "Resume" : "Pause")) ec.workspace.toggle_pause_play_mode();
        }

        const float right_x = std::max(width - 275.0f, ImGui::GetCursorPosX() + 16.0f);
        ImGui::SameLine(right_x);
        const bool dirty = ec.state.docs.session && ec.state.docs.session->is_dirty();
        draw_status_pill(dirty ? "Unsaved" : "Saved", dirty ? t.status_warn : t.status_ok);
        if (dirty) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Save")) (void)ec.workspace.save_document();
        }
        ImGui::SameLine(0.0f, 10.0f);
        const char* preset_name = ec.state.vis.preset == WorkspacePreset::Focus ? "Focus  v" : "Advanced  v";
        if (ImGui::Button(preset_name)) ImGui::OpenPopup("##WorkspaceMenu");
        draw_workspace_menu_popup(ec);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void draw_left_rail(EditorPanelContext& ec, float top, float height) {
    const EditorThemeTokens& t = editor_theme();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    ImGui::SetNextWindowPos(ImVec2(0.0f, top));
    ImGui::SetNextWindowSize(ImVec2(t.rail_width, height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, t.bg_base);
    if (ImGui::Begin("##LeftRail", nullptr, flags)) {
        auto& v = ec.state.vis;
        if (draw_rail_icon_button("##r_ws", RailIcon::Workspace, "Workspace (Focus)", v.active_rail == 0)) {
            v.active_rail = 0;
            v.scene_tree = !v.scene_tree;
        }
        if (draw_rail_icon_button("##r_as", RailIcon::Assets, "Content Shelf (Assets)", v.content_shelf)) {
            v.active_rail = 1;
            v.content_shelf = !v.content_shelf;
        }
        if (draw_rail_icon_button("##r_wd", RailIcon::World, "World Authoring (.world / Cells)", v.world_panel)) {
            v.active_rail = 2;
            v.world_panel = !v.world_panel;
        }
        if (draw_rail_icon_button("##r_tl", RailIcon::Tools, "Advanced Tools (Profiler / Render / Console)",
                                  v.preset == WorkspacePreset::Advanced)) {
            v.active_rail = 3;
            ec.workspace.set_workspace_preset(v.preset == WorkspacePreset::Focus ? WorkspacePreset::Advanced
                                                                                 : WorkspacePreset::Focus);
        }
        if (draw_rail_icon_button("##r_ai", RailIcon::Agent, "Astral Agent Workspace", v.agent)) {
            v.active_rail = 4;
            v.agent = !v.agent;
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (draw_rail_icon_button("##r_in", RailIcon::Inspector, "Contextual Inspector (I)", v.inspector)) {
            v.inspector = !v.inspector;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void draw_status_bar(EditorPanelContext& ec, float y, float width) {
    const EditorThemeTokens& t = editor_theme();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    ImGui::SetNextWindowPos(ImVec2(0.0f, y));
    ImGui::SetNextWindowSize(ImVec2(width, t.statusbar_height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 3.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, t.bg_base);
    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        const std::size_t pending = ec.state.docs.session ? ec.state.docs.session->pending_change_count() : 0u;
        const std::string doc_name =
            ec.state.docs.world_mode && ec.state.docs.world_authoring && ec.state.docs.world_authoring->is_open()
                ? ec.state.docs.world_authoring->world_path().filename().string()
                : (ec.state.docs.session && ec.state.docs.session->is_open()
                       ? ec.state.docs.session->document().path.filename().string()
                       : "none");
        const float ms = ec.app.profiler ? ec.app.profiler->last_frame_ms() : 0.0f;
        const float fps = ms > 0.01f ? (1000.0f / ms) : 0.0f;
        ImGui::TextDisabled("%s   |   Doc: %s   |   Pending: %zu   |   Agent: %s   |   %.0f FPS",
                            ec.state.ui.status_line.c_str(), doc_name.c_str(), pending,
                            ec.state.docs.tasks ? std::string(agent_task_state_name(ec.state.docs.tasks->state())).c_str()
                                                : "Idle",
                            static_cast<double>(fps));
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void build_dock_layout(ImGuiID dockspace_id, ImVec2 size, WorkspacePreset preset) {
#ifdef IMGUI_HAS_DOCK
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, size);

    ImGuiID main_id = dockspace_id;
    const float tree_ratio = std::clamp(230.0f / std::max(size.x, 400.0f), 0.14f, 0.24f);
    const float agent_ratio = std::clamp(310.0f / std::max(size.x, 400.0f), 0.18f, 0.28f);

    ImGuiID left_id = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Left, tree_ratio, nullptr, &main_id);
    ImGuiID right_id = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Right, agent_ratio, nullptr, &main_id);
    ImGuiID bottom_id = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Down, 0.22f, nullptr, &main_id);

    ImGui::DockBuilderDockWindow("Scene", left_id);
    ImGui::DockBuilderDockWindow("Viewport", main_id);
    ImGui::DockBuilderDockWindow("Astral Agent", right_id);
    ImGui::DockBuilderDockWindow("Inspector", right_id);
    ImGui::DockBuilderDockWindow("Content", bottom_id);

    if (preset == WorkspacePreset::Advanced) {
        ImGui::DockBuilderDockWindow("World Authoring", left_id);
        ImGui::DockBuilderDockWindow("Render", right_id);
        ImGui::DockBuilderDockWindow("Materials", right_id);
        ImGui::DockBuilderDockWindow("Console", bottom_id);
        ImGui::DockBuilderDockWindow("Profiler", bottom_id);
    }
    ImGui::DockBuilderFinish(dockspace_id);
#else
    (void)dockspace_id;
    (void)size;
    (void)preset;
#endif
}

void draw_command_palette(EditorPanelContext& ec) {
    if (!ec.state.vis.command_palette) return;
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.22f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 340.0f), ImGuiCond_Always);
    if (ImGui::Begin("Command Palette##Modal", &ec.state.vis.command_palette,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "%s", ec.state.ui.palette_query.c_str());
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##PaletteSearch", "Type a command or entity name...", buf, sizeof buf)) {
            ec.state.ui.palette_query = buf;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ec.state.vis.command_palette = false;
        ImGui::Separator();

        auto item = [&](const char* label, const char* shortcut, auto&& fn) {
            if (!icontains(label, ec.state.ui.palette_query)) return;
            if (ImGui::Selectable(label)) {
                fn();
                ec.state.vis.command_palette = false;
            }
            if (shortcut && *shortcut) {
                ImGui::SameLine(ImGui::GetWindowWidth() - 90.0f);
                ImGui::TextDisabled("%s", shortcut);
            }
        };
        item("Save Document", "Ctrl+S", [&] { (void)ec.workspace.save_document(); });
        item("Undo", "Ctrl+Z", [&] { (void)ec.workspace.undo(ec.app); });
        item("Redo", "Ctrl+Y", [&] { (void)ec.workspace.redo(ec.app); });
        item("Switch to Focus Workspace", "", [&] { ec.workspace.set_workspace_preset(WorkspacePreset::Focus); });
        item("Switch to Advanced Workspace", "", [&] { ec.workspace.set_workspace_preset(WorkspacePreset::Advanced); });
        item("Reset Workspace Layout", "", [&] { ec.workspace.reset_workspace_layout(); });
        item("Toggle Contextual Inspector", "I", [&] { ec.state.vis.inspector = !ec.state.vis.inspector; });
        item("Toggle Content Shelf", "", [&] { ec.state.vis.content_shelf = !ec.state.vis.content_shelf; });
        item("Play / Stop Scene", "", [&] {
            if (ec.workspace.play_state() == PlayState::Stopped) (void)ec.workspace.enter_play_mode(ec.app);
            else ec.workspace.stop_play_mode(ec.app);
        });
        item("Focus Selected Entity", "F", [&] { ec.workspace.focus_selected(ec.app); });

        if (ec.state.docs.session && ec.state.docs.session->is_open()) {
            if (const Json* ents = ec.state.docs.session->document().root.find("entities"); ents && ents->is_array()) {
                ImGui::SeparatorText("Entities");
                for (std::size_t i = 0; i < ents->size(); ++i) {
                    const auto id = static_cast<std::uint32_t>((*ents)[i]["id"].as_number(0.0));
                    std::string name = "#" + std::to_string(id);
                    if (const Json* c = (*ents)[i].find("components"); c && c->find("astral::Name")) {
                        name += " " + (*c)["astral::Name"]["value"].as_string();
                    }
                    if (!icontains(name, ec.state.ui.palette_query)) continue;
                    if (ImGui::Selectable(name.c_str())) {
                        ec.workspace.select_entity({id, {}});
                        ec.workspace.focus_selected(ec.app);
                        ec.state.vis.command_palette = false;
                    }
                }
            }
        }
    }
    ImGui::End();
}

void draw_modals(EditorPanelContext& ec) {
    if (ec.state.vis.conflict_modal) {
        ImGui::OpenPopup("External File Change");
        ec.state.vis.conflict_modal = false;
    }
    if (ImGui::BeginPopupModal("External File Change", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", ec.state.ui.conflict_message.c_str());
        ImGui::TextDisabled("Choose whether to reload from disk or overwrite with your in-editor changes.");
        if (ImGui::Button("Reload from Disk")) {
            std::string err;
            if (ec.state.docs.session && ec.state.docs.session->reload(&err)) sync_scene_to_world_live(ec);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Overwrite Disk")) {
            (void)ec.workspace.save_document(nullptr, true);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ec.state.vis.autosave_modal) {
        ImGui::OpenPopup("Recover Autosave");
        ec.state.vis.autosave_modal = false;
    }
    if (ImGui::BeginPopupModal("Recover Autosave", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("An unsaved crash-recovery snapshot was found for this document.");
        if (ImGui::Button("Restore Autosave")) {
            std::string err;
            if (ec.state.docs.session && ec.state.docs.session->restore_autosave(&err)) sync_scene_to_world_live(ec);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard Snapshot")) {
            if (ec.state.docs.session) ec.state.docs.session->clear_autosave();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ec.state.vis.shortcuts_modal) {
        if (ImGui::Begin("Keyboard Shortcuts", &ec.state.vis.shortcuts_modal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::BulletText("Ctrl+K : Command Palette");
            ImGui::BulletText("Ctrl+S : Save Document");
            ImGui::BulletText("Ctrl+Z / Ctrl+Y : Undo / Redo");
            ImGui::BulletText("RMB + WASD / QE : Fly Camera in Viewport");
            ImGui::BulletText("Q / W / E / R : Select / Translate / Rotate / Scale");
            ImGui::BulletText("F : Focus Selected Entity");
            ImGui::BulletText("I : Toggle Contextual Inspector");
            ImGui::BulletText("Delete : Delete Selected Entity");
        }
        ImGui::End();
    }
}

} // namespace

void draw_editor_shell(EditorPanelContext& ec) {
    if (!ec.state.vis.theme_applied) {
        apply_editor_theme();
        ec.state.vis.theme_applied = true;
    }
    handle_global_shortcuts(ec);

    const EditorThemeTokens& t = editor_theme();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float body_h = std::max(100.0f, display.y - t.toolbar_height - t.statusbar_height);
    const float dock_w = std::max(100.0f, display.x - t.rail_width);

    draw_top_toolbar(ec, display.x);
    draw_left_rail(ec, t.toolbar_height, body_h);
    draw_status_bar(ec, display.y - t.statusbar_height, display.x);

    // Responsive auto-collapse of bottom shelf on narrow screens when first building Focus layout.
    if (ec.state.vis.layout_dirty && ec.state.vis.preset == WorkspacePreset::Focus && display.x <= 1280.0f) {
        ec.state.vis.content_shelf = false;
    }

    const ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    ImGui::SetNextWindowPos(ImVec2(t.rail_width, t.toolbar_height));
    ImGui::SetNextWindowSize(ImVec2(dock_w, body_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, t.bg_base);
    if (ImGui::Begin("##DockHost", nullptr, host_flags)) {
#ifdef IMGUI_HAS_DOCK
        const ImGuiID dock_id = ImGui::GetID("AstralEditorDockSpace");
        if (ec.state.vis.layout_dirty) {
            build_dock_layout(dock_id, ImVec2(dock_w, body_h), ec.state.vis.preset);
            ec.state.vis.layout_dirty = false;
        }
        ImGui::DockSpace(dock_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
#endif
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ec.state.panels.draw_visible(ec);
    draw_command_palette(ec);
    draw_modals(ec);
}

} // namespace astral::editor
