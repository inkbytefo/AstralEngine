#include "features/cubes/cubes.hpp"

using namespace astral;

namespace cubes {

namespace {

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

} // namespace

void install_cubes(FeatureContext& feature) {
    feature.component<Spin>();
    feature.component<Bob>();
    feature.system<SpinSystem>(Phase::Update);
}

void spawn_grid(Context& ctx, int grid) {
    for (int x = 0; x < grid; ++x) {
        for (int z = 0; z < grid; ++z) {
            const float fx = static_cast<float>(x - grid / 2) * 1.6f;
            const float fz = static_cast<float>(z - grid / 2) * 1.6f;
            const float hue = static_cast<float>(x + z) / (2.0f * static_cast<float>(grid));
            ctx.world.create(Transform{.position = {fx, 0, fz}, .scale = {0.8f, 0.8f, 0.8f}},
                             MeshRenderer{ctx.meshes.cube, ctx.create_material({.base_color = {0.9f - hue * 0.6f, 0.4f + hue * 0.4f, 0.3f + hue * 0.6f, 1}})},
                             Spin{normalize(Vec3{fx, 3, fz}), 0.5f + hue * 2.0f},
                             Bob{.height = 0.4f, .phase = fx * 0.3f + fz * 0.2f});
        }
    }
    ctx.world.create(Transform{.position = {0, -1, 0}, .scale = {60, 1, 60}},
                     MeshRenderer{ctx.meshes.plane, ctx.create_material({.base_color = {0.25f, 0.27f, 0.3f, 1}})});
}

} // namespace cubes
