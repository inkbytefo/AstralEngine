// sdf_scene — the hybrid renderer: meshes and signed-distance-field shapes in one lit scene.
// SDF blobs melt into each other (smooth union), a box is carved by a sphere (subtract),
// coloured point lights orbit, and the sun casts shadows from both meshes and SDFs.
#include <astral/astral.hpp>

#include <cmath>

using namespace astral;

struct Orbit {
    float radius = 3.0f;
    float speed = 1.0f;
    float height = 1.0f;
    float phase = 0.0f;
};

class OrbitSystem final : public System {
public:
    void update(Context& ctx) override {
        const auto t = static_cast<float>(ctx.time.elapsed);
        ctx.world.each<Transform, const Orbit>([t](Transform& tr, const Orbit& o) {
            const float a = t * o.speed + o.phase;
            tr.position = {std::cos(a) * o.radius, o.height + std::sin(a * 1.7f) * 0.4f, std::sin(a) * o.radius};
        });
    }
};

class SpinSystem final : public System {
public:
    void update(Context& ctx) override {
        const float dt = ctx.time.delta;
        ctx.world.each<Transform, const Sdf>([dt](Transform& tr, const Sdf& s) {
            if (s.shape.primitive == SdfPrimitive::Torus) {
                tr.rotation = normalize(Quat::from_axis_angle({1, 0.3f, 0}, dt * 0.8f) * tr.rotation);
            }
        });
        if (ctx.input.pressed(Key::Escape)) ctx.quit();
    }
};

class SdfScene final : public Game {
public:
    void on_start(Context& ctx) override {
        auto& w = ctx.world;
        // Ground and a few metallic mesh spheres.
        w.create(Transform{.scale = {40, 1, 40}}, MeshRenderer{ctx.meshes.plane, {0.35f, 0.36f, 0.4f, 1}, 0.8f});
        for (int i = 0; i < 5; ++i) {
            const float a = static_cast<float>(i) * 1.2566f;
            w.create(Transform{.position = {std::cos(a) * 6.5f, 0.6f, std::sin(a) * 6.5f}, .scale = {1.2f, 1.2f, 1.2f}},
                     MeshRenderer{ctx.meshes.sphere, {0.95f, 0.75f, 0.35f, 1}, 0.25f, 1.0f});
        }

        // Three blobs that melt together while orbiting (smooth union).
        const Vec4 blob_colors[3] = {{0.9f, 0.25f, 0.2f, 1}, {0.2f, 0.6f, 0.95f, 1}, {0.95f, 0.85f, 0.25f, 1}};
        for (int i = 0; i < 3; ++i) {
            w.create(Transform{}, Orbit{.radius = 0.9f, .speed = 0.7f + 0.2f * static_cast<float>(i), .height = 1.6f,
                                        .phase = static_cast<float>(i) * 2.1f},
                     Sdf{.shape = {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::SmoothUnion,
                                   .size = {0.75f, 0, 0, 0}, .blend = 0.9f, .color = blob_colors[i], .roughness = 0.3f},
                         .order = 0});
        }
        // A rounded box carved by a sphere.
        w.create(Transform{.position = {-3.5f, 1.0f, 2.5f}},
                 Sdf{.shape = {.primitive = SdfPrimitive::Box, .operation = SdfOperation::Union,
                               .size = {1, 1, 1, 0.12f}, .color = {0.85f, 0.85f, 0.9f, 1}, .roughness = 0.5f},
                     .order = 10});
        w.create(Transform{.position = {-3.5f, 1.6f, 3.2f}},
                 Sdf{.shape = {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::SmoothSubtract,
                               .size = {0.9f, 0, 0, 0}, .blend = 0.1f},
                     .order = 11});
        // A spinning metallic torus.
        w.create(Transform{.position = {3.5f, 1.4f, 2.0f}},
                 Sdf{.shape = {.primitive = SdfPrimitive::Torus, .operation = SdfOperation::Union,
                               .size = {0.9f, 0.3f, 0, 0}, .color = {0.9f, 0.9f, 0.95f, 1}, .roughness = 0.15f, .metallic = 1.0f},
                     .order = 20});

        // Orbiting coloured lights.
        const Vec3 light_colors[3] = {{1, 0.3f, 0.2f}, {0.2f, 0.5f, 1}, {0.3f, 1, 0.4f}};
        for (int i = 0; i < 3; ++i) {
            w.create(Transform{}, Orbit{.radius = 5.0f, .speed = 0.5f, .height = 2.5f, .phase = static_cast<float>(i) * 2.094f},
                     PointLight{.color = light_colors[i], .intensity = 25.0f, .radius = 9.0f});
        }

        w.create(Transform{.position = {0, 6, 11}, .rotation = Quat::from_axis_angle({1, 0, 0}, -0.45f)}, Camera{});
        ctx.sun = {.direction = {-0.5f, -1.0f, -0.35f}, .intensity = 1.6f};
        ctx.environment.ambient = 0.6f;
        ctx.environment.exposure = 0.9f;

        ctx.add_system<OrbitSystem>(Phase::Update);
        ctx.add_system<SpinSystem>(Phase::Update);
    }
};

int main() {
    App app({.title = "Astral — SDF scene"});
    return app.run<SdfScene>();
}
