#include <astral/editor/workspace.hpp>

#include <astral/world/scene.hpp>
#include <astral/world/transform.hpp>

#include <algorithm>
#include <cmath>

namespace astral::editor {

EditorWorkspace::EditorWorkspace(EditorWorkspaceDesc desc)
    : desc_(std::move(desc)), default_agent_(std::make_unique<PrefabAssistantAgent>()) {}

EditorWorkspace::~EditorWorkspace() = default;

void EditorWorkspace::push_console(std::string line) {
    console_.push_back(std::move(line));
    if (console_.size() > 256) console_.erase(console_.begin());
}

void EditorWorkspace::refresh_asset_list() {
    asset_files_ = fs_.list_files(desc_.asset_root, {".scene", ".prefab", ".world"});
}

void EditorWorkspace::clear_scene_entities(World& world) {
    std::vector<Entity> doomed;
    world.for_each_entity([&](Entity e) {
        if (e != camera_) doomed.push_back(e);
    });
    for (Entity e : doomed) {
        if (world.alive(e)) world.destroy(e);
    }
}

void EditorWorkspace::ensure_editor_camera(Context& ctx) {
    if (camera_.valid() && ctx.world.alive(camera_)) return;
    camera_ = ctx.world.create(
        Name{"EditorCamera"},
        Transform{.position = {0.0f, 3.5f, 8.0f}, .rotation = Quat::from_axis_angle({1, 0, 0}, cam_pitch_)},
        Camera{.fov_y = radians(50.0f), .near_plane = 0.1f, .far_plane = 500.0f, .active = true});
}

void EditorWorkspace::on_start(Context& ctx) {
    ctx.show_debug_ui = true;
    session_ = std::make_unique<EditorSession>(fs_, ctx.types, desc_.asset_root);
    review_ = std::make_unique<ProposalReview>(*session_);
    ensure_editor_camera(ctx);
    refresh_asset_list();
    if (!desc_.initial_document.empty()) {
        std::string err;
        if (!open_document(ctx, desc_.initial_document, &err)) {
            push_console("error: " + err);
            status_line_ = err;
        }
    }
}

void EditorWorkspace::on_update(Context& ctx) {
    if (!camera_.valid() || !ctx.world.alive(camera_)) return;
    auto* t = ctx.world.get<Transform>(camera_);
    if (!t) return;
    const float speed = 6.0f * ctx.time.delta;
    const Quat q = Quat::from_axis_angle({0, 1, 0}, cam_yaw_) * Quat::from_axis_angle({1, 0, 0}, cam_pitch_);
    t->rotation = q;
    const Vec3 forward = q * Vec3{0, 0, -1};
    const Vec3 right = q * Vec3{1, 0, 0};
    if (ctx.input.down(Key::W)) t->position += forward * speed;
    if (ctx.input.down(Key::S)) t->position -= forward * speed;
    if (ctx.input.down(Key::D)) t->position += right * speed;
    if (ctx.input.down(Key::A)) t->position -= right * speed;
}

void EditorWorkspace::on_debug_ui(Context& ctx) { draw_panels(ctx); }

void EditorWorkspace::on_shutdown(Context&) {}

bool EditorWorkspace::open_document(Context& ctx, const std::filesystem::path& relative_or_full_path, std::string* error) {
    if (!session_) {
        if (error) *error = "workspace not started";
        return false;
    }
    const std::filesystem::path full =
        relative_or_full_path.is_absolute() ? relative_or_full_path : (desc_.asset_root / relative_or_full_path);
    if (!session_->open(full, error)) return false;
    selected_ = {};
    preview_mode_ = false;
    if (review_) review_->reject("opened another document");
    push_console("opened " + full.string() + " (rev " + std::to_string(session_->document().revision.number) + ")");
    status_line_ = "Opened " + full.filename().string();
    return reload_live_scene(ctx, error);
}

bool EditorWorkspace::reload_live_scene(Context& ctx, std::string* error) {
    if (!session_ || !session_->is_open()) {
        if (error) *error = "no document open";
        return false;
    }
    clear_scene_entities(ctx.world);
    const std::string fmt = session_->document().root["format"].as_string();
    if (fmt != "astral.scene") return true;

    // Re-read the document and its referenced prefabs directly from disk so reload is immediate and exact.
    std::string io_err;
    auto fresh_doc = load_editor_document(fs_, session_->document().path, &io_err);
    if (!fresh_doc) {
        if (error) *error = io_err;
        push_console("reload error: " + io_err);
        return false;
    }
    std::unordered_map<std::string, Json> prefab_cache;
    PrefabLibrary live_prefabs(ctx.types, [&](std::string_view rel_path) -> SourceDocument {
        const std::string key(rel_path);
        if (auto it = prefab_cache.find(key); it != prefab_cache.end()) return {&it->second, 1, {}};
        const std::filesystem::path full = desc_.asset_root / key;
        std::string err;
        auto text = fs_.read_text(full, &err);
        if (!text) return {nullptr, 0, err};
        auto parsed = Json::parse(*text, &err);
        if (!parsed) return {nullptr, 0, err};
        auto [ins, _] = prefab_cache.emplace(key, std::move(*parsed));
        return {&ins->second, 1, {}};
    });
    const LoadResult loaded = astral::load_scene(ctx.world, live_prefabs, fresh_doc->root);
    if (!loaded.errors.empty()) {
        if (error) *error = loaded.errors.front();
        for (const std::string& e : loaded.errors) push_console("reload error: " + e);
        return false;
    }
    push_console("reloaded live scene (" + std::to_string(loaded.entities.size()) + " entities)");
    return true;
}

bool EditorWorkspace::request_ai_proposal(std::string prompt, std::string* error) {
    if (!review_ || !default_agent_) {
        if (error) *error = "editor review service not initialized";
        return false;
    }
    if (!review_->request(*default_agent_, selected_, std::move(prompt), error)) {
        if (error) push_console("AI proposal failed: " + *error);
        return false;
    }
    preview_mode_ = true;
    push_console("AI proposed: " + review_->active()->summary + " (" + review_->preview()->diff.summary() + ")");
    status_line_ = "Reviewing AI proposal (" + review_->preview()->diff.summary() + ")";
    return true;
}

TransactionResult EditorWorkspace::approve_proposal(Context& ctx) {
    if (!review_) return {.error = "review not initialized"};
    const TransactionResult res = review_->approve();
    if (!res.ok) {
        push_console("approve failed: " + res.error);
        status_line_ = res.error;
        return res;
    }
    preview_mode_ = false;
    refresh_asset_list();
    std::string reload_err;
    (void)reload_live_scene(ctx, &reload_err);
    push_console("approved AI proposal -> rev " + std::to_string(res.new_revision.number));
    status_line_ = "Approved & reloaded (rev " + std::to_string(res.new_revision.number) + ")";
    return res;
}

void EditorWorkspace::reject_proposal(std::string_view reason) {
    if (!review_) return;
    review_->reject(reason);
    preview_mode_ = false;
    push_console("rejected AI proposal");
    status_line_ = "Proposal rejected";
}

TransactionResult EditorWorkspace::undo(Context& ctx) {
    if (!session_) return {.error = "session not initialized"};
    const TransactionResult res = session_->undo();
    if (res.ok) {
        std::string err;
        (void)reload_live_scene(ctx, &err);
        push_console("undo -> rev " + std::to_string(res.new_revision.number));
        status_line_ = "Undone";
    }
    return res;
}

TransactionResult EditorWorkspace::redo(Context& ctx) {
    if (!session_) return {.error = "session not initialized"};
    const TransactionResult res = session_->redo();
    if (res.ok) {
        std::string err;
        (void)reload_live_scene(ctx, &err);
        push_console("redo -> rev " + std::to_string(res.new_revision.number));
        status_line_ = "Redone";
    }
    return res;
}

} // namespace astral::editor
