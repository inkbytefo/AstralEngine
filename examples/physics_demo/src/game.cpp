#include "game.hpp"

#include "features/lifetime/lifetime.hpp"
#include "features/player/player.hpp"
#include "features/rain/rain.hpp"
#include "features/shooter/shooter.hpp"

using namespace astral;

namespace physics_demo {

bool install_features(Context& ctx) {
    FeatureSet game;
    game.add("lifetime", lifetime::install_lifetime);
    game.add("player", player::install_player);
    game.add("shooter", shooter::install_shooter);
    game.add("rain", rain::install_rain);
    return game.install(ctx);
}

void PhysicsGame::on_start(Context& ctx) {
    if (!install_features(ctx)) {
        ctx.quit();
        return;
    }
    auto& w = ctx.world;
    // Ground (static) and a pyramid of dynamic crates.
    w.create(Transform{.position = {0, -0.5f, 0}, .scale = {60, 1, 60}},
             MeshRenderer{ctx.meshes.cube, ctx.create_material({.base_color = {0.35f, 0.37f, 0.4f, 1}, .roughness = 0.9f})},
             RigidBody{.desc = {.shape = ShapeDesc::box({30, 0.5f, 30}), .motion = MotionType::Static}});
    constexpr int base = 6;
    for (int row = 0; row < base; ++row) {
        for (int i = 0; i < base - row; ++i) {
            const float x = (static_cast<float>(i) - static_cast<float>(base - row - 1) * 0.5f) * 1.05f;
            const float y = 0.5f + static_cast<float>(row) * 1.0f;
            w.create(Transform{.position = {x, y, -4}},
                     MeshRenderer{ctx.meshes.cube, ctx.create_material({.base_color = {0.85f, 0.6f, 0.3f, 1}, .roughness = 0.7f})},
                     RigidBody{.desc = {.shape = ShapeDesc::box({0.5f, 0.5f, 0.5f}), .friction = 0.7f}});
        }
    }
    // The player: the controller's position is at the feet, so the visible body is a child
    // entity lifted to the capsule's center.
    const Entity body = w.create(Transform{.position = {0, 0, 6}}, CharacterController{});
    w.create(Transform{.position = {0, 0.9f, 0}, .scale = {0.8f, 1.8f, 0.8f}}, Parent{body},
             MeshRenderer{ctx.meshes.sphere, ctx.create_material({.base_color = {0.3f, 0.9f, 0.45f, 1}, .roughness = 0.4f})});
    w.create(Transform{}, Camera{}, player::PlayerCamera{});
    w.create(rain::RainSpawner{});
    ctx.sun = {.direction = {-0.4f, -1.0f, -0.5f}, .intensity = 2.0f};
    ctx.log.info("WASD walk, Space jump, arrows turn, left click shoot, right click push, Esc quit");
}

} // namespace physics_demo
