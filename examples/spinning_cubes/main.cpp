// spinning_cubes — what a game looks like on AstralEngine: components are plain structs,
// systems hold the logic, the App runs the loop. No Vulkan, no threads, no window code.
#include <astral/astral.hpp>

#include <cmath>

using namespace astral;

// --- Components: just data -------------------------------------------------------------
struct Spin {
    Vec3 axis{0, 1, 0};
    float speed = 1.0f; // radians per second
};

struct Bob {
    float height = 0.5f;
    float phase = 0.0f;
    float base_y = 0.0f;
};

struct FlyCamera {
    float speed = 8.0f;
    float yaw = 0.0f;
    float pitch = -0.3f;
};

// --- Systems: the logic ----------------------------------------------------------------
class SpinSystem final : public System {
public:
    void update(Context& ctx) override {
        // Runs on all worker threads: each entity only touches its own data.
        const float dt = ctx.time.delta;
        ctx.world.parallel_each<Transform, const Spin>(ctx.jobs, [dt](Transform& t, const Spin& s) {
            t.rotation = normalize(Quat::from_axis_angle(s.axis, s.speed * dt) * t.rotation);
        });
    }
};

class BobSystem final : public System {
public:
    void update(Context& ctx) override {
        const auto time = static_cast<float>(ctx.time.elapsed);
        ctx.world.each<Transform, const Bob>([time](Transform& t, const Bob& b) {
            t.position.y = b.base_y + std::sin(time * 2.0f + b.phase) * b.height;
        });
    }
};

class FlyCameraSystem final : public System {
public:
    void update(Context& ctx) override {
        if (ctx.input.pressed(Key::Escape)) ctx.quit();
        ctx.world.each<Transform, FlyCamera>([&](Transform& t, FlyCamera& cam) {
            if (ctx.input.down(MouseButton::Right)) {
                cam.yaw -= ctx.input.mouse_delta().x * 0.003f;
                cam.pitch -= ctx.input.mouse_delta().y * 0.003f;
            }
            t.rotation = Quat::from_axis_angle({0, 1, 0}, cam.yaw) * Quat::from_axis_angle({1, 0, 0}, cam.pitch);
            const Vec2 move = ctx.input.wasd();
            const Vec3 forward = t.rotation * Vec3{0, 0, -1};
            const Vec3 right = t.rotation * Vec3{1, 0, 0};
            t.position += (forward * move.y + right * move.x) * cam.speed * ctx.time.delta;
        });
    }
};

// --- The game: set up the scene and register systems -----------------------------------
class SpinningCubes final : public Game {
public:
    void on_start(Context& ctx) override {
        constexpr int grid = 24;
        for (int x = 0; x < grid; ++x) {
            for (int z = 0; z < grid; ++z) {
                const float fx = static_cast<float>(x - grid / 2) * 1.6f;
                const float fz = static_cast<float>(z - grid / 2) * 1.6f;
                const float hue = static_cast<float>(x + z) / (2.0f * grid);
                ctx.world.create(Transform{.position = {fx, 0, fz}, .scale = {0.8f, 0.8f, 0.8f}},
                                 MeshRenderer{ctx.meshes.cube, ctx.create_material({.base_color = {0.9f - hue * 0.6f, 0.4f + hue * 0.4f, 0.3f + hue * 0.6f, 1}})},
                                 Spin{normalize(Vec3{fx, 3, fz}), 0.5f + hue * 2.0f},
                                 Bob{.height = 0.4f, .phase = fx * 0.3f + fz * 0.2f});
            }
        }
        ctx.world.create(Transform{.position = {0, -1, 0}, .scale = {60, 1, 60}},
                         MeshRenderer{ctx.meshes.plane, ctx.create_material({.base_color = {0.25f, 0.27f, 0.3f, 1}})});
        ctx.world.create(Transform{.position = {0, 9, 26}}, Camera{}, FlyCamera{});

        ctx.add_system<FlyCameraSystem>(Phase::PreUpdate);
        ctx.add_system<SpinSystem>(Phase::Update);
        ctx.add_system<BobSystem>(Phase::Update);
        ctx.log.info("{} cubes — WASD to fly, hold right mouse to look, Esc to quit", grid * grid);
    }
};

int main() {
    App app({.title = "Astral — spinning cubes"});
    return app.run<SpinningCubes>();
}
