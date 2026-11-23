#include "services/play_host.hpp"

#include <astral/world/scene.hpp>
#include <astral/world/transform.hpp>

#include <algorithm>
#include <unordered_map>

namespace astral::editor {

PlayHost::~PlayHost() = default;

bool PlayHost::start(Context& ctx, const Json& document_root, const IEditorFileSystem& fs,
                     const std::filesystem::path& asset_root, std::string* error) {
    stop(ctx);
    auto play_world = std::make_unique<World>();
    std::unordered_map<std::string, Json> cache;
    PrefabLibrary prefabs(ctx.types, [&](std::string_view rel) -> SourceDocument {
        const std::string key(rel);
        if (auto it = cache.find(key); it != cache.end()) return {&it->second, 1, {}};
        std::string err;
        auto text = fs.read_text(asset_root / key, &err);
        if (!text) return {nullptr, 0, err};
        auto parsed = Json::parse(*text, &err);
        if (!parsed) return {nullptr, 0, err};
        auto [ins, _] = cache.emplace(key, std::move(*parsed));
        return {&ins->second, 1, {}};
    });
    const LoadResult loaded = load_scene(*play_world, prefabs, document_root);
    if (!loaded.errors.empty()) {
        if (error) *error = loaded.errors.front();
        return false;
    }

    // Ensure an active camera exists inside the play world (copy editor camera if none in scene).
    bool has_camera = false;
    play_world->each<const Camera>([&](const Camera& c) { has_camera |= c.active; });
    if (!has_camera) {
        ctx.world.each<const Camera, const Transform>([&](const Camera& c, const Transform& t) {
            if (!has_camera && c.active) {
                play_world->create(Name{"PlayCamera"}, t, c);
                has_camera = true;
            }
        });
    }

    auto play_physics = std::make_unique<PhysicsWorld>(PhysicsDesc{.logger = &ctx.log});
    std::vector<std::pair<Entity, BodyHandle>> play_bodies;
    play_world->each<const RigidBody, const Transform>([&](Entity e, const RigidBody& rb, const Transform& t) {
        BodyDesc desc = rb.desc;
        desc.position = t.position;
        desc.rotation = t.rotation;
        desc.user_data = e.index;
        if (const BodyHandle bh = play_physics->create_body(desc); bh.valid() && desc.motion != MotionType::Static) {
            play_bodies.emplace_back(e, bh);
        }
    });
    update_transforms(*play_world);

    world_ = std::move(play_world);
    physics_ = std::move(play_physics);
    bodies_ = std::move(play_bodies);
    accumulator_ = 0.0f;
    state_ = PlayState::Playing;
    ctx.viewport().set_render_world(world_.get());
    return true;
}

void PlayHost::toggle_pause() noexcept {
    if (state_ == PlayState::Playing) state_ = PlayState::Paused;
    else if (state_ == PlayState::Paused) state_ = PlayState::Playing;
}

void PlayHost::stop(Context& ctx) {
    if (ctx.viewport().render_world() == world_.get()) ctx.viewport().set_render_world(nullptr);
    bodies_.clear();
    physics_.reset();
    world_.reset();
    accumulator_ = 0.0f;
    state_ = PlayState::Stopped;
}

void PlayHost::step(float dt) {
    if (state_ != PlayState::Playing || !world_) return;
    constexpr float fixed_dt = 1.0f / 60.0f;
    accumulator_ += std::min(dt, 0.1f);
    int steps = 0;
    while (accumulator_ >= fixed_dt && steps < 4) {
        if (physics_) {
            physics_->step(fixed_dt);
            for (const auto& [e, bh] : bodies_) {
                if (!world_->alive(e) || !physics_->valid(bh)) continue;
                if (auto* t = world_->get<Transform>(e)) {
                    t->position = physics_->position(bh);
                    t->rotation = physics_->rotation(bh);
                }
            }
        }
        accumulator_ -= fixed_dt;
        ++steps;
    }
    update_transforms(*world_);
}

} // namespace astral::editor
