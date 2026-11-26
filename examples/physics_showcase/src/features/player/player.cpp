#include "features/player/player.hpp"

#include "features/fly_camera/fly_camera.hpp"

#include <vector>

using namespace astral;

namespace player {

namespace {

/// F: hand the camera to the fly_camera feature (its rule moves any entity with fly::FlyCamera), or take it back.
void toggle_flight(Context& ctx) {
    if (!ctx.actions.pressed("toggle_fly")) return;
    std::vector<Entity> cameras;
    ctx.world.each<PlayerCamera>([&](Entity e, PlayerCamera&) { cameras.push_back(e); });
    for (const Entity e : cameras) {
        PlayerCamera& cam = *ctx.world.get<PlayerCamera>(e);
        cam.flying = !cam.flying;
        if (cam.flying) {
            ctx.world.add(e, fly::FlyCamera{.speed = 10.0f, .fast_speed = 30.0f, .yaw = cam.yaw, .pitch = cam.pitch});
        } else {
            ctx.world.remove<fly::FlyCamera>(e);
        }
    }
}

} // namespace

void install_player(FeatureContext& feature) {
    feature.component<PlayerCamera>();
    feature.system(Phase::PreUpdate, "player::ToggleFlight", toggle_flight, SystemAccess{}.query<PlayerCamera>().writes<fly::FlyCamera>());
    feature.system(
        Phase::PreUpdate, "player::CameraTurn",
        [](Context& ctx) {
            const float turn = ctx.actions.value("turn");
            ctx.world.each<Transform, PlayerCamera>([&](Transform& t, PlayerCamera& cam) {
                if (cam.flying) return;
                cam.yaw += turn * cam.turn_speed * ctx.time.delta;
                t.rotation = Quat::from_axis_angle({0, 1, 0}, cam.yaw) * Quat::from_axis_angle({1, 0, 0}, cam.pitch);
            });
        },
        SystemAccess{}.query<Transform, PlayerCamera>());
    feature.system(
        Phase::Update, "player::PlayerMove",
        [](Context& ctx) {
            const PlayerCamera* cam = ctx.singleton<PlayerCamera>();
            if (!cam) return;
            const Quat facing = Quat::from_axis_angle({0, 1, 0}, cam->yaw);
            const Vec2 move_input = cam->flying ? Vec2{} : ctx.actions.axis("move");
            const bool jumped = !cam->flying && ctx.actions.pressed("jump");
            Vec3 position{};
            ctx.world.each<CharacterController, const Transform>([&](CharacterController& c, const Transform& t) {
                c.move = (facing * Vec3{move_input.x, 0, -move_input.y}) * cam->walk_speed;
                c.jump = c.jump || jumped;
                position = t.position;
            });
            if (cam->flying) return;
            ctx.world.each<Transform, const PlayerCamera>([&](Transform& t, const PlayerCamera& c) {
                t.position = position + t.rotation * Vec3{0, 0, c.distance} + Vec3{0, c.height, 0};
            });
        },
        SystemAccess{}.query<Transform, CharacterController, const PlayerCamera>());
}

} // namespace player
