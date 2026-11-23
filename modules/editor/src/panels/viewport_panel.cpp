#include "editor_state.hpp"
#include "theme/editor_theme.hpp"

#include <astral/world/transform.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace astral::editor {

namespace {

void commit_transform_field(EditorPanelContext& ec, DocumentEntityId target, std::string_view field, Json val,
                            const std::string& coalesce_key) {
    EditorSession* s = ec.state.docs.session.get();
    if (!s || !s->is_open() || !target.valid()) return;
    Transaction tx;
    tx.summary = "Transform #" + target.to_string();
    tx.expected_revision = s->document().revision;
    tx.coalesce_key = coalesce_key;
    tx.commands.push_back({.kind = CommandKind::SetComponentField, .target = target,
                           .component = "astral::Transform", .field = std::string(field), .value = std::move(val)});
    if (s->apply(tx).ok) {
        if (auto it = ec.state.selection.record_to_entity.find(target.record_id);
            it != ec.state.selection.record_to_entity.end() && ec.app.world.alive(it->second)) {
            if (auto* tf = ec.app.world.get<Transform>(it->second)) {
                const Json* rec = s->document().find_record(target.record_id);
                if (rec) {
                    if (const Json* c = rec->find("components"); c && c->find("astral::Transform")) {
                        from_json((*c)["astral::Transform"], *tf, {});
                    }
                }
            }
        }
    }
}

void update_viewport_camera(EditorPanelContext& ec) {
    ViewportState& vp = ec.state.viewport;
    if (!vp.hovered || !ImGui::IsMouseDown(ImGuiMouseButton_Right)) return;
    const ImGuiIO& io = ImGui::GetIO();
    vp.yaw -= io.MouseDelta.x * 0.0045f;
    vp.pitch = std::clamp(vp.pitch - io.MouseDelta.y * 0.0045f, -1.50f, 1.50f);

    const Quat q = Quat::from_axis_angle({0, 1, 0}, vp.yaw) * Quat::from_axis_angle({1, 0, 0}, vp.pitch);
    const Vec3 fwd = q * Vec3{0, 0, -1};
    const Vec3 right = q * Vec3{1, 0, 0};
    const Vec3 up{0, 1, 0};
    const float boost = io.KeyShift ? 2.5f : 1.0f;
    const float step = vp.fly_speed * boost * io.DeltaTime;
    if (ImGui::IsKeyDown(ImGuiKey_W)) vp.cam_pos += fwd * step;
    if (ImGui::IsKeyDown(ImGuiKey_S)) vp.cam_pos -= fwd * step;
    if (ImGui::IsKeyDown(ImGuiKey_D)) vp.cam_pos += right * step;
    if (ImGui::IsKeyDown(ImGuiKey_A)) vp.cam_pos -= right * step;
    if (ImGui::IsKeyDown(ImGuiKey_E)) vp.cam_pos += up * step;
    if (ImGui::IsKeyDown(ImGuiKey_Q)) vp.cam_pos -= up * step;
}

void apply_gizmo_drag(EditorPanelContext& ec, const PickRay& ray, Vec2 uv) {
    ViewportState& vp = ec.state.viewport;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        vp.drag.active = false;
        return;
    }
    const float cur_t = project_ray_onto_axis(ray, vp.drag.start_position, vp.drag.start_axis_dir);
    const float delta = apply_snap(cur_t - vp.drag.start_axis_t, vp.snap_enabled, vp.snap_step);
    if (vp.gizmo == GizmoMode::Translate) {
        const Vec3 p = vp.drag.start_position + vp.drag.start_axis_dir * delta;
        Json arr = Json::array();
        arr.push_back(p.x);
        arr.push_back(p.y);
        arr.push_back(p.z);
        commit_transform_field(ec, vp.drag.target, "position", std::move(arr), vp.drag.coalesce_key);
    } else if (vp.gizmo == GizmoMode::Scale) {
        Vec3 scl = vp.drag.start_scale;
        const float factor = std::max(0.05f, 1.0f + delta * 0.35f);
        if (vp.drag.axis == 0) scl.x *= factor;
        else if (vp.drag.axis == 1) scl.y *= factor;
        else scl.z *= factor;
        Json arr = Json::array();
        arr.push_back(scl.x);
        arr.push_back(scl.y);
        arr.push_back(scl.z);
        commit_transform_field(ec, vp.drag.target, "scale", std::move(arr), vp.drag.coalesce_key);
    } else if (vp.gizmo == GizmoMode::Rotate) {
        const float angle = apply_snap((uv.x - vp.drag.start_uv.x) * 4.5f, vp.snap_enabled, radians(15.0f));
        const Quat r = normalize(Quat::from_axis_angle(vp.drag.start_axis_dir, angle) * vp.drag.start_rotation);
        Json arr = Json::array();
        arr.push_back(r.x);
        arr.push_back(r.y);
        arr.push_back(r.z);
        arr.push_back(r.w);
        commit_transform_field(ec, vp.drag.target, "rotation", std::move(arr), vp.drag.coalesce_key);
    }
}

void update_gizmo_and_picking(EditorPanelContext& ec, ImVec2 img_min, ImVec2 img_size) {
    ViewportState& vp = ec.state.viewport;
    if (ec.workspace.play_state() != PlayState::Stopped) return;
    if (img_size.x <= 1.0f || img_size.y <= 1.0f) return;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const Vec2 uv{(mouse.x - img_min.x) / img_size.x, (mouse.y - img_min.y) / img_size.y};
    const CameraData cam = compute_editor_camera(vp);
    const PickRay ray = viewport_uv_to_ray(cam, uv);

    if (vp.hovered && std::abs(ImGui::GetIO().MouseWheel) > 0.01f && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        const Quat q = Quat::from_axis_angle({0, 1, 0}, vp.yaw) * Quat::from_axis_angle({1, 0, 0}, vp.pitch);
        vp.cam_pos += (q * Vec3{0, 0, -1}) * (ImGui::GetIO().MouseWheel * 0.9f);
    }
    if (vp.drag.active) {
        apply_gizmo_drag(ec, ray, uv);
        return;
    }
    if (!vp.hovered || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
    if (vp.gizmo != GizmoMode::Select && ec.state.selection.primary.valid()) {
        if (auto it = ec.state.selection.record_to_entity.find(ec.state.selection.primary.record_id);
            it != ec.state.selection.record_to_entity.end() && ec.app.world.alive(it->second)) {
            if (const auto* tf = ec.app.world.get<const Transform>(it->second)) {
                const float gscale = std::max(length(vp.cam_pos - tf->position), 1.5f) * 0.16f;
                const int axis = pick_gizmo_axis(ray, tf->position, tf->rotation, vp.space, gscale);
                if (axis >= 0) {
                    const Vec3 base = axis == 0 ? Vec3{1, 0, 0} : (axis == 1 ? Vec3{0, 1, 0} : Vec3{0, 0, 1});
                    vp.drag.active = true;
                    vp.drag.axis = axis;
                    vp.drag.target = ec.state.selection.primary;
                    vp.drag.start_position = tf->position;
                    vp.drag.start_rotation = tf->rotation;
                    vp.drag.start_scale = tf->scale;
                    vp.drag.start_axis_dir = vp.space == TransformSpace::Local ? normalize(tf->rotation * base) : base;
                    vp.drag.start_axis_t = project_ray_onto_axis(ray, tf->position, vp.drag.start_axis_dir);
                    vp.drag.start_uv = uv;
                    vp.drag.coalesce_key = "gizmo:" + vp.drag.target.to_string() + ":" +
                                           std::to_string(static_cast<int>(vp.gizmo)) + ":" + std::to_string(axis) + ":" +
                                           std::to_string(ImGui::GetFrameCount());
                    return;
                }
            }
        }
    }
    const DocumentEntityId picked = ec.workspace.pick_entity_at(ec.app, uv);
    if (picked.valid() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ec.state.vis.inspector = true;
}

void draw_viewport_overlay_bar(EditorPanelContext& ec, ImVec2 img_min) {
    const EditorThemeTokens& t = editor_theme();
    ImGui::SetCursorScreenPos(ImVec2(img_min.x + 12.0f, img_min.y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(t.bg_base.x, t.bg_base.y, t.bg_base.z, 0.82f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
    const bool has_ai_preview = ec.state.docs.tasks && ec.state.docs.tasks->state() == AgentTaskState::Review &&
                                ec.state.docs.tasks->preview() != nullptr;
    if (ImGui::BeginChild("##VpTools", ImVec2(0.0f, 32.0f),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX, ImGuiWindowFlags_NoScrollbar)) {
        auto tool_btn = [&](const char* lbl, GizmoMode mode) {
            const bool active = ec.state.viewport.gizmo == mode;
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, t.accent);
            if (ImGui::SmallButton(lbl)) ec.state.viewport.gizmo = mode;
            if (active) ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, 4.0f);
        };
        tool_btn("Select", GizmoMode::Select);
        tool_btn("Move", GizmoMode::Translate);
        tool_btn("Rotate", GizmoMode::Rotate);
        tool_btn("Scale", GizmoMode::Scale);
        if (ImGui::SmallButton(ec.state.viewport.space == TransformSpace::World ? "World" : "Local")) {
            ec.state.viewport.space = ec.state.viewport.space == TransformSpace::World ? TransformSpace::Local
                                                                                       : TransformSpace::World;
        }
        ImGui::SameLine(0.0f, 4.0f);
        if (ImGui::SmallButton(ec.state.viewport.snap_enabled ? "Snap:On" : "Snap")) {
            ec.state.viewport.snap_enabled = !ec.state.viewport.snap_enabled;
        }
        ImGui::SameLine(0.0f, 4.0f);
        if (ImGui::SmallButton(ec.state.viewport.show_grid ? "Grid:On" : "Grid")) {
            ec.state.viewport.show_grid = !ec.state.viewport.show_grid;
        }
        if (has_ai_preview) {
            ImGui::SameLine(0.0f, 10.0f);
            const bool after = ec.state.viewport.preview_view == ViewportPreviewMode::CandidateAfter;
            if (!after) ImGui::PushStyleColor(ImGuiCol_Button, t.accent);
            if (ImGui::SmallButton("Before")) ec.state.viewport.preview_view = ViewportPreviewMode::Live;
            if (!after) ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, 3.0f);
            if (after) ImGui::PushStyleColor(ImGuiCol_Button, t.accent);
            if (ImGui::SmallButton("After (AI)")) ec.state.viewport.preview_view = ViewportPreviewMode::CandidateAfter;
            if (after) ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

class ViewportPanel final : public EditorPanel {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "viewport"; }
    [[nodiscard]] std::string_view title() const noexcept override { return "Viewport"; }
    [[nodiscard]] bool is_visible(const EditorPanelContext&) const noexcept override { return true; }

    void draw(EditorPanelContext& ec) override {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const ImVec2 fb = ImGui::GetIO().DisplayFramebufferScale;
            const auto pw = static_cast<std::uint32_t>(std::max(64.0f, std::floor(avail.x * fb.x)));
            const auto ph = static_cast<std::uint32_t>(std::max(64.0f, std::floor(avail.y * fb.y)));
            ec.app.viewport().set_size(pw, ph);

            const ImVec2 img_min = ImGui::GetCursorScreenPos();
            ec.state.viewport.rect_min = {img_min.x, img_min.y};
            ec.state.viewport.rect_size = {std::max(avail.x, 1.0f), std::max(avail.y, 1.0f)};

            const TextureHandle tex = ec.app.viewport().texture();
            if (tex.valid()) {
                ImGui::Image(static_cast<ImTextureID>(pack_ui_texture(tex)), avail);
            } else {
                ImGui::Dummy(avail);
            }

            // Accept dragged .prefab assets dropped onto the 3D viewport.
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASTRAL_ASSET_PATH")) {
                    const std::string rel(static_cast<const char*>(payload->Data),
                                          static_cast<std::size_t>(payload->DataSize > 0 ? payload->DataSize - 1 : 0));
                    if (rel.ends_with(".prefab") && ec.state.docs.session && ec.state.docs.session->is_open()) {
                        Transaction tx;
                        tx.summary = "Drop prefab " + rel;
                        tx.expected_revision = ec.state.docs.session->document().revision;
                        tx.commands.push_back({.kind = CommandKind::InstantiatePrefab, .asset_path = rel});
                        if (ec.state.docs.session->apply(tx).ok) sync_scene_to_world_live(ec);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ec.state.viewport.hovered = ImGui::IsItemHovered();
            ec.state.viewport.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            update_viewport_camera(ec);
            update_gizmo_and_picking(ec, img_min, avail);
            draw_viewport_overlay_bar(ec, img_min);
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
};

} // namespace

std::unique_ptr<EditorPanel> make_viewport_panel() { return std::make_unique<ViewportPanel>(); }

} // namespace astral::editor
