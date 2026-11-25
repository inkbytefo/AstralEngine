#include "game.hpp"

#include <utility>
#include <vector>

using namespace astral;

namespace vfx_lab {

void VfxLabGame::on_start(Context& ctx) {
    const auto scene = ctx.scenes().load_scene("courtyard.scene");
    if (!scene.errors.empty()) {
        ctx.quit();
        return;
    }
    if (!config_.baseline) ctx.environment.fog_extinction = config_.fog;
    if (!config_.aerial || config_.baseline) ctx.environment.aerial_extinction = 0.0f;
    if (auto* sky = ctx.singleton_mut<Atmosphere>()) {
        sky->hour = config_.hour;
        sky->hours_per_second = config_.hours_per_second;
        sky->enabled = !config_.baseline;
    }
    if (config_.smoke || config_.vfx) {
        std::vector<std::pair<Entity, Vec3>> colliders;
        ctx.world.each<const PrimitiveMesh, const Transform>([&](Entity e, const PrimitiveMesh& mesh, const Transform& t) {
            if (mesh.shape == Primitive::Cube) colliders.emplace_back(e, t.scale * 0.5f);
        });
        for (const auto& [e, half] : colliders) {
            ctx.world.add(e, RigidBody{.desc = {.shape = ShapeDesc::box(half), .motion = MotionType::Static}});
        }
        if (config_.vfx) {
            play_vfx(ctx, "courtyard.vfx", {-2.8f, 1.05f, -2.4f});
        } else {
            ctx.world.create(Transform{.position = {-2.8f, 1.05f, -2.4f}},
                             GameplaySmoke({.radius_cells = 5,
                                            .cell_size_m = 0.4f,
                                            .extinction_per_m = 1.7f,
                                            .spread_interval_steps = 1}));
        }
    }
    ctx.world.create(
        Transform{.position = {0, 3.5f, 11.0f}, .rotation = Quat::from_axis_angle({1, 0, 0}, radians(-8.0f))},
        Camera{.fov_y = radians(58.0f)});
}

void VfxLabGame::on_shutdown(Context& ctx) {
    if (!config_.profile || !ctx.renderer) return;
    const auto stats = ctx.renderer->stats();
    ctx.log.info("vfx_lab volume bricks: {}, budget fallbacks: {}", stats.volume_bricks, stats.volume_brick_fallbacks);
    for (const auto& timing : ctx.renderer->gpu_timings()) {
        ctx.log.info("vfx_lab pass {}: {} ms", timing.name, timing.ms);
    }
}

} // namespace vfx_lab
