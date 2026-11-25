#include "game.hpp"

using namespace astral;

namespace terrain_lab {

void TerrainLabGame::on_start(Context& ctx) {
    const auto scene = ctx.scenes().load_scene("terrain.scene");
    for (const auto& error : scene.errors) ctx.log.error("terrain lab: {}", error);
    ctx.sun = {.direction = normalize(Vec3{-0.5f, -0.8f, -0.3f}), .intensity = 3.0f};
    ctx.environment.ambient = 0.45f;
    const Vec3 eye = far_view_ ? Vec3{64, 108, 190} : Vec3{64, 93, 158};
    ctx.world.create(Transform{.position = eye, .rotation = Quat::from_axis_angle({1, 0, 0}, radians(-43.0f))},
                     Camera{.fov_y = radians(55.0f)});
}

} // namespace terrain_lab
