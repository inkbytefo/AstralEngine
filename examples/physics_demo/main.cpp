// physics_demo — rigid bodies, raycasts and a character controller on AstralEngine.
// A box pyramid, balls raining down, left click shoots a ball from the camera, right click
// knocks back whatever the crosshair ray hits. All game code is components + systems.
#include <astral/astral.hpp>

#include <cmath>

using namespace astral;

struct FlyCamera {
    float yaw = 0.0f;
    float pitch = -0.35f;
};

struct Lifetime {
    float seconds = 10.0f;
};

class CameraSystem final : public System {
public:
    void update(Context& ctx) override {
        if (ctx.input.pressed(Key::Escape)) ctx.quit();
        ctx.world.each<Transform, FlyCamera>([&](Transform& t, FlyCamera& cam) {
            if (ctx.input.down(Key::Left)) cam.yaw += ctx.time.delta * 1.5f;
            if (ctx.input.down(Key::Right)) cam.yaw -= ctx.time.delta * 1.5f;
            t.rotation = Quat::from_axis_angle({0, 1, 0}, cam.yaw) * Quat::from_axis_angle({1, 0, 0}, cam.pitch);
        });
    }
};

/// WASD walks the character (Space jumps); the camera follows it.
class PlayerSystem final : public System {
public:
    void update(Context& ctx) override {
        Vec3 player_position{};
        float yaw = 0.0f;
        ctx.world.each<const FlyCamera>([&](const FlyCamera& cam) { yaw = cam.yaw; });
        const Quat facing = Quat::from_axis_angle({0, 1, 0}, yaw);
        ctx.world.each<CharacterController, const Transform>([&](CharacterController& c, const Transform& t) {
            const Vec2 input = ctx.input.wasd();
            c.move = (facing * Vec3{input.x, 0, -input.y}) * 6.0f;
            c.jump = c.jump || ctx.input.pressed(Key::Space);
            player_position = t.position;
        });
        ctx.world.each<Transform, const FlyCamera>([&](Transform& t, const FlyCamera&) {
            t.position = player_position + t.rotation * Vec3{0, 0, 9} + Vec3{0, 1.6f, 0};
        });
    }
};

class ShootSystem final : public System {
public:
    void update(Context& ctx) override {
        Vec3 origin{}, forward{0, 0, -1};
        ctx.world.each<const Transform, const Camera>([&](const Transform& t, const Camera&) {
            origin = t.position;
            forward = t.rotation * Vec3{0, 0, -1};
        });
        if (ctx.input.pressed(MouseButton::Left)) {
            ctx.world.create(Transform{.position = origin + forward * 1.5f, .scale = {0.6f, 0.6f, 0.6f}},
                             MeshRenderer{ctx.meshes.sphere, {0.95f, 0.3f, 0.2f, 1}, 0.3f},
                             RigidBody{.desc = {.shape = ShapeDesc::sphere(0.3f), .linear_velocity = forward * 30.0f,
                                                .mass = 20.0f, .continuous = true}},
                             Lifetime{});
        }
        if (ctx.input.pressed(MouseButton::Right)) {
            if (const auto hit = ctx.physics->raycast(origin, forward, 200.0f)) {
                if (const RigidBody* rb = ctx.world.get<RigidBody>(entity_of(*hit))) {
                    ctx.physics->add_impulse(rb->handle, forward * 400.0f);
                }
            }
        }
    }
};

/// A new ball every half second, and old ones expire.
class RainSystem final : public System {
public:
    void update(Context& ctx) override {
        timer_ += ctx.time.delta;
        if (timer_ > 0.5f) {
            timer_ = 0;
            const float a = static_cast<float>(ctx.time.frame) * 0.37f;
            ctx.world.create(Transform{.position = {std::cos(a) * 3.0f, 14.0f, std::sin(a) * 3.0f - 2.0f}},
                             MeshRenderer{ctx.meshes.sphere, {0.3f, 0.6f, 0.95f, 1}, 0.25f, 0.0f},
                             RigidBody{.desc = {.shape = ShapeDesc::sphere(0.5f), .restitution = 0.4f}}, Lifetime{});
        }
        ctx.world.each<Lifetime>([&](Entity e, Lifetime& l) {
            l.seconds -= ctx.time.delta;
            if (l.seconds <= 0.0f) ctx.world.commands().destroy(e);
        });
    }

private:
    float timer_ = 0.0f;
};

class PhysicsDemo final : public Game {
public:
    void on_start(Context& ctx) override {
        auto& w = ctx.world;
        // Ground (static) and a pyramid of dynamic crates.
        w.create(Transform{.position = {0, -0.5f, 0}, .scale = {60, 1, 60}},
                 MeshRenderer{ctx.meshes.cube, {0.35f, 0.37f, 0.4f, 1}, 0.9f},
                 RigidBody{.desc = {.shape = ShapeDesc::box({30, 0.5f, 30}), .motion = MotionType::Static}});
        constexpr int base = 6;
        for (int row = 0; row < base; ++row) {
            for (int i = 0; i < base - row; ++i) {
                const float x = (static_cast<float>(i) - static_cast<float>(base - row - 1) * 0.5f) * 1.05f;
                const float y = 0.5f + static_cast<float>(row) * 1.0f;
                w.create(Transform{.position = {x, y, -4}},
                         MeshRenderer{ctx.meshes.cube, {0.85f, 0.6f, 0.3f, 1}, 0.7f},
                         RigidBody{.desc = {.shape = ShapeDesc::box({0.5f, 0.5f, 0.5f}), .friction = 0.7f}});
            }
        }
        // The player: the controller's position is at the feet, so the visible body is a child
        // entity lifted to the capsule's center.
        const Entity player = w.create(Transform{.position = {0, 0, 6}}, CharacterController{});
        w.create(Transform{.position = {0, 0.9f, 0}, .scale = {0.8f, 1.8f, 0.8f}}, Parent{player},
                 MeshRenderer{ctx.meshes.sphere, {0.3f, 0.9f, 0.45f, 1}, 0.4f});
        w.create(Transform{}, Camera{}, FlyCamera{});
        ctx.sun = {.direction = {-0.4f, -1.0f, -0.5f}, .intensity = 2.0f};

        ctx.add_system<CameraSystem>(Phase::PreUpdate);
        ctx.add_system<PlayerSystem>(Phase::Update);
        ctx.add_system<ShootSystem>(Phase::Update);
        ctx.add_system<RainSystem>(Phase::Update);
        ctx.log.info("WASD walk, Space jump, arrows turn, left click shoot, right click push, Esc quit");
    }
};

int main() {
    App app({.title = "Astral — physics demo"});
    return app.run<PhysicsDemo>();
}
