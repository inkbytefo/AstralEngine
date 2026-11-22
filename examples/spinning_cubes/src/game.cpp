#include "game.hpp"

#include "features/cubes/cubes.hpp"
#include "features/fly_camera/fly_camera.hpp"

using namespace astral;

namespace spinning_cubes {

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("fly_camera", fly::install_fly_camera);
    game.add("cubes", cubes::install_cubes);
    return game.install(ctx);
}

void CubesGame::on_start(Context& ctx) {
    if (!install_features(ctx)) {
        ctx.quit();
        return;
    }
    cubes::spawn_grid(ctx, 24);
    ctx.world.create(Transform{.position = {0, 9, 26}}, Camera{}, fly::FlyCamera{.speed = 8.0f, .fast_speed = 24.0f, .pitch = -0.3f});
    ctx.log.info("{} cubes — WASD to fly, Q/E down/up, hold right mouse to look, Esc to quit", 24 * 24);
}

} // namespace spinning_cubes
