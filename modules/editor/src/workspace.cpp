#include "editor_state.hpp"

#include <astral/world/scene.hpp>
#include <astral/world/transform.hpp>

#include <algorithm>
#include <cmath>

namespace astral::editor {

namespace {

void clear_non_camera_entities(World& world, Entity camera) {
    std::vector<Entity> doomed;
    world.for_each_entity([&](Entity e) {
        if (e != camera) doomed.push_back(e);
    });
    for (Entity e : doomed) {
        if (world.alive(e)) world.destroy(e);
    }
}

void ensure_editor_camera(Context& ctx, ViewportState& vp) {
    if (vp.camera.valid() && ctx.world.alive(vp.camera)) return;
    const Quat q = Quat::from_axis_angle({0, 1, 0}, vp.yaw) * Quat::from_axis_angle({1, 0, 0}, vp.pitch);
    vp.camera = ctx.world.create(
        Name{"EditorCamera"},
        Transform{.position = vp.cam_pos, .rotation = q},
        Camera{.fov_y = radians(50.0f), .near_plane = 0.1f, .far_plane = 500.0f, .active = true});
}

void index_live_entities(World& world, SelectionState& sel) {
    sel.record_to_entity.clear();
    world.each<const SceneId>([&](Entity e, const SceneId& sid) {
        if (sid.value != 0) sel.record_to_entity[sid.value] = e;
    });
}

void record_recent(std::vector<std::filesystem::path>& recent, const std::filesystem::path& p) {
    std::erase(recent, p);
    recent.insert(recent.begin(), p);
    if (recent.size() > 8) recent.resize(8);
}

void apply_preset_visibility(ShellVisibility& v, WorkspacePreset preset) {
    v.preset = preset;
    v.layout_dirty = true;
    v.scene_tree = true;
    v.agent = true;
    if (preset == WorkspacePreset::Focus) {
        v.active_rail = 0;
        v.content_shelf = false;
        v.inspector = false;
        v.world_panel = false;
        v.console = false;
        v.profiler = false;
        v.render_settings = false;
        v.materials = false;
    } else {
        v.active_rail = 3;
        v.content_shelf = true;
        v.inspector = true;
        v.console = true;
        v.profiler = true;
        v.render_settings = true;
        v.materials = true;
    }
}

} // namespace

CameraData compute_editor_camera(const ViewportState& vp) {
    const Quat q = Quat::from_axis_angle({0, 1, 0}, vp.yaw) * Quat::from_axis_angle({1, 0, 0}, vp.pitch);
    const Mat4 world_mat = compose(vp.cam_pos, q, {1, 1, 1});
    const float aspect = vp.rect_size.y > 1.0f ? (vp.rect_size.x / vp.rect_size.y) : (16.0f / 9.0f);
    return {
        .view = inverse(world_mat),
        .projection = perspective(radians(50.0f), aspect, 0.1f, 500.0f),
        .position = vp.cam_pos,
    };
}

IAgentProvider& active_agent_provider(DocumentServices& docs) {
    if (docs.provider_index == 1 && docs.codex_agent) return *docs.codex_agent;
    return *docs.builtin_agent;
}

void sync_scene_to_world_live(EditorPanelContext& ec) {
    EditorSession* s = ec.state.docs.session.get();
    if (!s || !s->is_open()) return;
    if (s->document().root["format"].as_string() != "astral.scene") return;

    clear_non_camera_entities(ec.app.world, ec.state.viewport.camera);
    std::unordered_map<std::string, Json> prefab_cache;
    PrefabLibrary prefabs(ec.app.types, [&](std::string_view rel_path) -> SourceDocument {
        const std::string key(rel_path);
        if (auto it = prefab_cache.find(key); it != prefab_cache.end()) return {&it->second, 1, {}};
        std::string err;
        auto text = ec.state.docs.fs.read_text(ec.state.desc.asset_root / key, &err);
        if (!text) return {nullptr, 0, err};
        auto parsed = Json::parse(*text, &err);
        if (!parsed) return {nullptr, 0, err};
        auto [ins, _] = prefab_cache.emplace(key, std::move(*parsed));
        return {&ins->second, 1, {}};
    });
    (void)astral::load_scene(ec.app.world, prefabs, s->document().root);
    update_transforms(ec.app.world);
    index_live_entities(ec.app.world, ec.state.selection);
}

EditorWorkspace::EditorWorkspace(EditorWorkspaceDesc desc) : impl_(std::make_unique<Impl>(std::move(desc))) {}
EditorWorkspace::~EditorWorkspace() = default;

void EditorWorkspace::on_start(Context& ctx) {
    ctx.show_debug_ui = true;
    auto& s = *impl_;
    s.docs.session = std::make_unique<EditorSession>(s.docs.fs, ctx.types, s.desc.asset_root);
    s.docs.session->set_persist_policy(s.desc.manual_save ? PersistPolicy::ManualSave : PersistPolicy::Immediate);
    s.docs.world_authoring = std::make_unique<WorldAuthoringSession>(s.docs.fs, ctx.types, s.desc.asset_root);
    s.world_ui.tools = default_placement_tools();
    s.docs.review = std::make_unique<ProposalReview>(*s.docs.session);
    s.docs.tasks = std::make_unique<AgentTaskManager>(*s.docs.session);
    register_editor_panels(s.panels);
    apply_preset_visibility(s.vis, s.desc.initial_preset);
    ensure_editor_camera(ctx, s.viewport);
    refresh_asset_list();
    if (!s.desc.initial_document.empty()) {
        std::string err;
        if (!open_document(ctx, s.desc.initial_document, &err)) {
            push_console("error: " + err);
            s.ui.status_line = err;
        }
    }
}

void EditorWorkspace::on_update(Context& ctx) {
    auto& s = *impl_;
    if (s.docs.tasks) s.docs.tasks->poll();

    if (s.play.active()) {
        s.play.step(ctx.time.delta);
        return;
    }

    // Periodic autosave sidecar for crash recovery when dirty in ManualSave mode.
    if (s.desc.manual_save && s.docs.session && s.docs.session->is_open() && s.docs.session->is_dirty()) {
        s.ui.autosave_timer += ctx.time.delta;
        if (s.ui.autosave_timer >= 30.0f) {
            s.ui.autosave_timer = 0.0f;
            (void)s.docs.session->write_autosave();
        }
    }

    // Sync editor camera entity transform.
    ensure_editor_camera(ctx, s.viewport);
    if (auto* t = ctx.world.get<Transform>(s.viewport.camera)) {
        t->position = s.viewport.cam_pos;
        t->rotation = Quat::from_axis_angle({0, 1, 0}, s.viewport.yaw) * Quat::from_axis_angle({1, 0, 0}, s.viewport.pitch);
    }

    // Update alternate AI candidate 3D preview world binding cleanly.
    const PreviewResult* pv = s.docs.tasks ? s.docs.tasks->preview() : nullptr;
    if (s.viewport.preview_view == ViewportPreviewMode::CandidateAfter && pv && pv->preview_world) {
        ctx.viewport().set_render_world(pv->preview_world.get());
        const CameraData cam = compute_editor_camera(s.viewport);
        ctx.viewport().set_camera_override(&cam);
    } else {
        ctx.viewport().set_render_world(nullptr);
        ctx.viewport().set_camera_override(nullptr);
        Entity sel_ent{};
        if (auto it = s.selection.record_to_entity.find(s.selection.primary.record_id);
            it != s.selection.record_to_entity.end()) {
            sel_ent = it->second;
        }
        draw_viewport_overlays(ctx.debug, ctx.world, sel_ent, s.viewport.cam_pos, s.viewport.show_grid,
                               s.viewport.gizmo, s.viewport.space, s.viewport.drag.active ? s.viewport.drag.axis : -1);
    }
}

void EditorWorkspace::on_debug_ui(Context& ctx) {
    EditorPanelContext ec{*this, *impl_, ctx};
    draw_editor_shell(ec);
}

void EditorWorkspace::on_shutdown(Context& ctx) {
    impl_->play.stop(ctx);
    ctx.viewport().set_render_world(nullptr);
    ctx.viewport().set_camera_override(nullptr);
    ctx.viewport().set_size(0, 0);
    if (impl_->docs.tasks) impl_->docs.tasks->cancel();
}

bool EditorWorkspace::open_document(Context& ctx, const std::filesystem::path& relative_or_full_path, std::string* error) {
    auto& s = *impl_;
    if (!s.docs.session) {
        if (error) *error = "workspace not started";
        return false;
    }
    stop_play_mode(ctx);
    const std::filesystem::path full =
        relative_or_full_path.is_absolute() ? relative_or_full_path : (s.desc.asset_root / relative_or_full_path);
    if (full.extension() == ".world") return open_world_map(ctx, full, error);
    if (!s.docs.session->open(full, error)) return false;
    s.docs.world_mode = false;
    s.selection.primary = {};
    s.selection.multi.clear();
    s.ui.preview_mode = false;
    s.viewport.preview_view = ViewportPreviewMode::Live;
    if (s.docs.review) s.docs.review->reject("opened another document");
    if (s.docs.tasks) s.docs.tasks->discard("opened another document");
    if (s.desc.manual_save && s.docs.session->has_autosave()) s.vis.autosave_modal = true;
    record_recent(s.ui.recent_assets, full);
    push_console("opened " + full.string() + " (rev " + std::to_string(s.docs.session->document().revision.number) + ")");
    s.ui.status_line = "Opened " + full.filename().string();
    return reload_live_scene(ctx, error);
}

bool EditorWorkspace::open_world_map(Context& ctx, const std::filesystem::path& relative_or_full_path, std::string* error) {
    auto& s = *impl_;
    if (!s.docs.world_authoring) {
        if (error) *error = "workspace not started";
        return false;
    }
    stop_play_mode(ctx);
    const std::filesystem::path full =
        relative_or_full_path.is_absolute() ? relative_or_full_path : (s.desc.asset_root / relative_or_full_path);
    if (!s.docs.world_authoring->open_map(full, error)) return false;
    s.docs.world_mode = true;
    s.world_ui.selected_instance_id = 0;
    s.vis.world_panel = true;
    record_recent(s.ui.recent_assets, full);
    push_console("opened world map " + full.string() + " (" +
                 std::to_string(s.docs.world_authoring->raw_entities().size()) + " source .ent files)");
    s.ui.status_line = "World: " + full.filename().string();
    return reload_world_preview(ctx, error);
}

bool EditorWorkspace::reload_world_preview(Context& ctx, std::string* error) {
    auto& s = *impl_;
    if (!s.docs.world_authoring || !s.docs.world_authoring->is_open()) {
        if (error) *error = "no world map open";
        return false;
    }
    clear_non_camera_entities(ctx.world, s.viewport.camera);
    std::vector<std::string> expand_errors;
    const MapSource expanded = s.docs.world_authoring->build_expanded_source(&expand_errors);
    for (const std::string& e : expand_errors) push_console("world warning: " + e);

    Json preview_scene = Json::object();
    preview_scene["format"] = "astral.scene";
    preview_scene["version"] = 1.0;
    Json entities = Json::array();
    for (const MapEntity& me : expanded.entities) entities.push_back(me.record);
    preview_scene["entities"] = std::move(entities);

    std::unordered_map<std::string, Json> prefab_cache;
    PrefabLibrary live_prefabs(ctx.types, [&](std::string_view rel_path) -> SourceDocument {
        const std::string key(rel_path);
        if (auto it = prefab_cache.find(key); it != prefab_cache.end()) return {&it->second, 1, {}};
        std::string err;
        auto text = s.docs.fs.read_text(s.desc.asset_root / key, &err);
        if (!text) return {nullptr, 0, err};
        auto parsed = Json::parse(*text, &err);
        if (!parsed) return {nullptr, 0, err};
        auto [ins, _] = prefab_cache.emplace(key, std::move(*parsed));
        return {&ins->second, 1, {}};
    });
    const LoadResult loaded = astral::load_scene(ctx.world, live_prefabs, preview_scene);
    const WorldPosition origin = s.docs.world_authoring->preview_origin();
    if (origin.x != 0.0 || origin.y != 0.0 || origin.z != 0.0) {
        for (Entity e : loaded.entities) {
            if (auto* tf = ctx.world.get<Transform>(e); tf && !ctx.world.has<Parent>(e)) {
                tf->position.x -= static_cast<float>(origin.x);
                tf->position.y -= static_cast<float>(origin.y);
                tf->position.z -= static_cast<float>(origin.z);
            }
        }
    }
    update_transforms(ctx.world);
    index_live_entities(ctx.world, s.selection);
    push_console("reloaded world preview (" + std::to_string(loaded.entities.size()) + " entities)");
    return true;
}

bool EditorWorkspace::reload_live_scene(Context& ctx, std::string* error) {
    auto& s = *impl_;
    if (!s.docs.session || !s.docs.session->is_open()) {
        if (error) *error = "no document open";
        return false;
    }
    clear_non_camera_entities(ctx.world, s.viewport.camera);
    const Json* source_root = &s.docs.session->document().root;
    std::optional<EditorDocument> fresh_doc;
    if (s.docs.session->persist_policy() == PersistPolicy::Immediate) {
        std::string io_err;
        fresh_doc = load_editor_document(s.docs.fs, s.docs.session->document().path, &io_err);
        if (!fresh_doc) {
            if (error) *error = io_err;
            push_console("reload error: " + io_err);
            return false;
        }
        source_root = &fresh_doc->root;
    }
    if ((*source_root)["format"].as_string() != "astral.scene") return true;

    std::unordered_map<std::string, Json> prefab_cache;
    PrefabLibrary live_prefabs(ctx.types, [&](std::string_view rel_path) -> SourceDocument {
        const std::string key(rel_path);
        if (auto it = prefab_cache.find(key); it != prefab_cache.end()) return {&it->second, 1, {}};
        std::string err;
        auto text = s.docs.fs.read_text(s.desc.asset_root / key, &err);
        if (!text) return {nullptr, 0, err};
        auto parsed = Json::parse(*text, &err);
        if (!parsed) return {nullptr, 0, err};
        auto [ins, _] = prefab_cache.emplace(key, std::move(*parsed));
        return {&ins->second, 1, {}};
    });
    const LoadResult loaded = astral::load_scene(ctx.world, live_prefabs, *source_root);
    if (!loaded.errors.empty()) {
        if (error) *error = loaded.errors.front();
        for (const std::string& e : loaded.errors) push_console("reload error: " + e);
        return false;
    }
    update_transforms(ctx.world);
    index_live_entities(ctx.world, s.selection);
    push_console("reloaded live scene (" + std::to_string(loaded.entities.size()) + " entities)");
    return true;
}

bool EditorWorkspace::save_document(std::string* error, bool force_overwrite) {
    auto& s = *impl_;
    if (!s.docs.session || !s.docs.session->is_open()) {
        if (error) *error = "no document open";
        return false;
    }
    std::string err;
    if (!s.docs.session->save(&err, force_overwrite)) {
        if (s.docs.session->has_external_conflict()) {
            s.ui.conflict_message = err;
            s.vis.conflict_modal = true;
        }
        if (error) *error = err;
        push_console("save error: " + err);
        s.ui.status_line = "Save failed: " + err;
        return false;
    }
    s.ui.autosave_timer = 0.0f;
    refresh_asset_list();
    push_console("saved " + s.docs.session->document().path.filename().string());
    s.ui.status_line = "Saved";
    return true;
}

bool EditorWorkspace::request_ai_proposal(std::string prompt, std::string* error) {
    auto& s = *impl_;
    if (!s.docs.review || !s.docs.builtin_agent) {
        if (error) *error = "editor review service not initialized";
        return false;
    }
    if (!s.docs.review->request(*s.docs.builtin_agent, s.selection.primary, std::move(prompt), error)) {
        if (error) push_console("AI proposal failed: " + *error);
        return false;
    }
    s.ui.preview_mode = true;
    push_console("AI proposed: " + s.docs.review->active()->summary + " (" + s.docs.review->preview()->diff.summary() + ")");
    s.ui.status_line = "Reviewing AI proposal (" + s.docs.review->preview()->diff.summary() + ")";
    return true;
}

TransactionResult EditorWorkspace::approve_proposal(Context& ctx) {
    auto& s = *impl_;
    if (!s.docs.review) return {.error = "review not initialized"};
    const TransactionResult res = s.docs.review->approve();
    if (!res.ok) {
        push_console("approve failed: " + res.error);
        s.ui.status_line = res.error;
        return res;
    }
    s.ui.preview_mode = false;
    refresh_asset_list();
    std::string reload_err;
    (void)reload_live_scene(ctx, &reload_err);
    push_console("approved AI proposal -> rev " + std::to_string(res.new_revision.number));
    s.ui.status_line = "Approved & reloaded (rev " + std::to_string(res.new_revision.number) + ")";
    return res;
}

void EditorWorkspace::reject_proposal(std::string_view reason) {
    auto& s = *impl_;
    if (!s.docs.review) return;
    s.docs.review->reject(reason);
    s.ui.preview_mode = false;
    push_console("rejected AI proposal");
    s.ui.status_line = "Proposal rejected";
}

TransactionResult EditorWorkspace::undo(Context& ctx) {
    auto& s = *impl_;
    if (s.docs.world_mode && s.docs.world_authoring && s.docs.world_authoring->is_open()) {
        std::string err;
        const bool ok = s.docs.world_authoring->undo(&err);
        if (ok) (void)reload_world_preview(ctx, &err);
        return {.ok = ok, .error = err};
    }
    if (!s.docs.session) return {.error = "session not initialized"};
    const TransactionResult res = s.docs.session->undo();
    if (res.ok) {
        EditorPanelContext ec{*this, s, ctx};
        sync_scene_to_world_live(ec);
        push_console("undo -> rev " + std::to_string(res.new_revision.number));
        s.ui.status_line = "Undone";
    }
    return res;
}

TransactionResult EditorWorkspace::redo(Context& ctx) {
    auto& s = *impl_;
    if (s.docs.world_mode && s.docs.world_authoring && s.docs.world_authoring->is_open()) {
        std::string err;
        const bool ok = s.docs.world_authoring->redo(&err);
        if (ok) (void)reload_world_preview(ctx, &err);
        return {.ok = ok, .error = err};
    }
    if (!s.docs.session) return {.error = "session not initialized"};
    const TransactionResult res = s.docs.session->redo();
    if (res.ok) {
        EditorPanelContext ec{*this, s, ctx};
        sync_scene_to_world_live(ec);
        push_console("redo -> rev " + std::to_string(res.new_revision.number));
        s.ui.status_line = "Redone";
    }
    return res;
}

void EditorWorkspace::select_entity(DocumentEntityId id) {
    impl_->selection.primary = id;
    impl_->selection.multi.clear();
    if (id.valid()) impl_->selection.multi.push_back(id);
}

const DocumentEntityId& EditorWorkspace::selected_entity() const noexcept { return impl_->selection.primary; }
const std::vector<DocumentEntityId>& EditorWorkspace::selected_entities() const noexcept { return impl_->selection.multi; }

DocumentEntityId EditorWorkspace::pick_entity_at(Context& ctx, Vec2 viewport_uv) {
    const CameraData cam = compute_editor_camera(impl_->viewport);
    const PickRay ray = viewport_uv_to_ray(cam, viewport_uv);
    const auto hit = pick_world_entity(ctx.world, ray, impl_->viewport.camera);
    if (hit) {
        select_entity(hit->doc_id);
        return hit->doc_id;
    }
    select_entity({});
    return {};
}

void EditorWorkspace::focus_selected(Context& ctx) {
    auto& s = *impl_;
    auto it = s.selection.record_to_entity.find(s.selection.primary.record_id);
    if (it == s.selection.record_to_entity.end() || !ctx.world.alive(it->second)) return;
    if (const auto* wt = ctx.world.get<const WorldTransform>(it->second)) {
        const Vec3 target{wt->matrix[3].x, wt->matrix[3].y, wt->matrix[3].z};
        const Quat q = Quat::from_axis_angle({0, 1, 0}, s.viewport.yaw) * Quat::from_axis_angle({1, 0, 0}, s.viewport.pitch);
        const Vec3 fwd = q * Vec3{0, 0, -1};
        s.viewport.cam_pos = target - fwd * 5.5f;
    }
}

void EditorWorkspace::set_workspace_preset(WorkspacePreset preset) { apply_preset_visibility(impl_->vis, preset); }
WorkspacePreset EditorWorkspace::workspace_preset() const noexcept { return impl_->vis.preset; }
void EditorWorkspace::reset_workspace_layout() { apply_preset_visibility(impl_->vis, impl_->vis.preset); }
void EditorWorkspace::open_inspector(bool open) { impl_->vis.inspector = open; }
bool EditorWorkspace::is_inspector_open() const noexcept { return impl_->vis.inspector; }
void EditorWorkspace::open_command_palette(bool open) { impl_->vis.command_palette = open; }

void EditorWorkspace::set_gizmo_mode(GizmoMode mode) noexcept { impl_->viewport.gizmo = mode; }
GizmoMode EditorWorkspace::gizmo_mode() const noexcept { return impl_->viewport.gizmo; }
void EditorWorkspace::set_transform_space(TransformSpace space) noexcept { impl_->viewport.space = space; }
TransformSpace EditorWorkspace::transform_space() const noexcept { return impl_->viewport.space; }
void EditorWorkspace::set_snap(bool enabled, float step) noexcept {
    impl_->viewport.snap_enabled = enabled;
    impl_->viewport.snap_step = step;
}

bool EditorWorkspace::enter_play_mode(Context& ctx, std::string* error) {
    auto& s = *impl_;
    if (!s.docs.session || !s.docs.session->is_open()) {
        if (error) *error = "no open scene to play";
        return false;
    }
    if (!s.play.start(ctx, s.docs.session->document().root, s.docs.fs, s.desc.asset_root, error)) return false;
    s.ui.status_line = "Playing (isolated copy)";
    push_console("entered Play mode (isolated world)");
    return true;
}

void EditorWorkspace::toggle_pause_play_mode() noexcept {
    impl_->play.toggle_pause();
    impl_->ui.status_line = impl_->play.state() == PlayState::Paused ? "Paused" : "Playing (isolated copy)";
}

void EditorWorkspace::stop_play_mode(Context& ctx) {
    if (!impl_->play.active()) return;
    impl_->play.stop(ctx);
    impl_->ui.status_line = "Stopped Play";
    push_console("exited Play mode");
}

PlayState EditorWorkspace::play_state() const noexcept { return impl_->play.state(); }

void EditorWorkspace::set_viewport_preview_mode(ViewportPreviewMode mode) noexcept { impl_->viewport.preview_view = mode; }
ViewportPreviewMode EditorWorkspace::viewport_preview_mode() const noexcept { return impl_->viewport.preview_view; }

EditorSession* EditorWorkspace::session() noexcept { return impl_->docs.session.get(); }
const EditorSession* EditorWorkspace::session() const noexcept { return impl_->docs.session.get(); }
WorldAuthoringSession* EditorWorkspace::world_authoring() noexcept { return impl_->docs.world_authoring.get(); }
const WorldAuthoringSession* EditorWorkspace::world_authoring() const noexcept { return impl_->docs.world_authoring.get(); }
const std::vector<PlacementToolDef>& EditorWorkspace::placement_tools() const noexcept { return impl_->world_ui.tools; }
ProposalReview* EditorWorkspace::review() noexcept { return impl_->docs.review.get(); }
const ProposalReview* EditorWorkspace::review() const noexcept { return impl_->docs.review.get(); }
AgentTaskManager* EditorWorkspace::agent_tasks() noexcept { return impl_->docs.tasks.get(); }
const AgentTaskManager* EditorWorkspace::agent_tasks() const noexcept { return impl_->docs.tasks.get(); }
const std::vector<std::string>& EditorWorkspace::console_lines() const noexcept { return impl_->ui.console; }
const std::vector<std::filesystem::path>& EditorWorkspace::asset_files() const noexcept { return impl_->ui.asset_files; }
bool EditorWorkspace::preview_mode() const noexcept { return impl_->ui.preview_mode; }
void EditorWorkspace::set_preview_mode(bool enabled) noexcept { impl_->ui.preview_mode = enabled; }

void EditorWorkspace::push_console(std::string line) {
    impl_->ui.console.push_back(std::move(line));
    if (impl_->ui.console.size() > 256) impl_->ui.console.erase(impl_->ui.console.begin());
}

void EditorWorkspace::refresh_asset_list() {
    impl_->ui.asset_files = impl_->docs.fs.list_files(impl_->desc.asset_root, {".scene", ".prefab", ".world", ".material", ".ashader"});
}

} // namespace astral::editor
