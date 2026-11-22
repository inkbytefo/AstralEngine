#include "features/player/player.hpp"

using namespace astral;

namespace player {

namespace {

/// The arrows turn the camera (the "turn" action).
class CameraTurn final : public System {
public:
    void update(Context& ctx) override {
        const float turn = ctx.actions.value("turn");
        ctx.world.each<Transform, PlayerCamera>([&](Transform& t, PlayerCamera& cam) {
            cam.yaw += turn * cam.turn_speed * ctx.time.delta;
            t.rotation = Quat::from_axis_angle({0, 1, 0}, cam.yaw) * Quat::from_axis_angle({1, 0, 0}, cam.pitch);
        });
    }
};

/// WASD walks the character (Space jumps); the camera follows it.
class PlayerMove final : public System {
public:
    void update(Context& ctx) override {
        Vec3 position{};
        float yaw = 0.0f, speed = 6.0f, distance = 9.0f, height = 1.6f;
        ctx.world.each<const PlayerCamera>([&](const PlayerCamera& cam) {
            yaw = cam.yaw;
            speed = cam.walk_speed;
            distance = cam.distance;
            height = cam.height;
        });
        const Quat facing = Quat::from_axis_angle({0, 1, 0}, yaw);
        ctx.world.each<CharacterController, const Transform>([&](CharacterController& c, const Transform& t) {
            const Vec2 input = ctx.actions.axis("move");
            c.move = (facing * Vec3{input.x, 0, -input.y}) * speed;
            c.jump = c.jump || ctx.actions.pressed("jump");
            position = t.position;
        });
        ctx.world.each<Transform, const PlayerCamera>([&](Transform& t, const PlayerCamera&) {
            t.position = position + t.rotation * Vec3{0, 0, distance} + Vec3{0, height, 0};
        });
    }
};

} // namespace

void install_player(FeatureContext& feature) {
    feature.component<PlayerCamera>();
    feature.system<CameraTurn>(Phase::PreUpdate);
    feature.system<PlayerMove>(Phase::Update);
}

} // namespace player
