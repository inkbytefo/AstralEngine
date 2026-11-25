#include "features/player/player.hpp"

using namespace astral;

namespace player {

void install_player(FeatureContext& feature) {
    feature.component<PlayerCamera>();
    feature.system(
        Phase::PreUpdate, "player::CameraTurn",
        [](Context& ctx) {
            const float turn = ctx.actions.value("turn");
            ctx.world.each<Transform, PlayerCamera>([&](Transform& t, PlayerCamera& cam) {
                cam.yaw += turn * cam.turn_speed * ctx.time.delta;
                t.rotation = Quat::from_axis_angle({0, 1, 0}, cam.yaw) * Quat::from_axis_angle({1, 0, 0}, cam.pitch);
            });
        },
        SystemAccess{}.query<Transform, PlayerCamera>());
    feature.system(
        Phase::Update, "player::PlayerMove",
        [](Context& ctx) {
            const PlayerCamera* cam = ctx.singleton<PlayerCamera>();
            const float yaw = cam ? cam->yaw : 0.0f;
            const float speed = cam ? cam->walk_speed : 6.0f;
            const float distance = cam ? cam->distance : 9.0f;
            const float height = cam ? cam->height : 1.6f;

            Vec3 position{};
            const Quat facing = Quat::from_axis_angle({0, 1, 0}, yaw);
            const Vec2 move_input = ctx.actions.axis("move");
            const bool jumped = ctx.actions.pressed("jump");
            ctx.world.each<CharacterController, const Transform>([&](CharacterController& c, const Transform& t) {
                c.move = (facing * Vec3{move_input.x, 0, -move_input.y}) * speed;
                c.jump = c.jump || jumped;
                position = t.position;
            });
            ctx.world.each<Transform, const PlayerCamera>([&](Transform& t, const PlayerCamera&) {
                t.position = position + t.rotation * Vec3{0, 0, distance} + Vec3{0, height, 0};
            });
        },
        SystemAccess{}.query<Transform, CharacterController, const PlayerCamera>());
}

} // namespace player
