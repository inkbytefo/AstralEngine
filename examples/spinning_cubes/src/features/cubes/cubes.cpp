#include "features/cubes/cubes.hpp"

#include <algorithm>
#include <vector>

using namespace astral;

namespace cubes {

void install_cubes(FeatureContext& feature) {
    feature.component<Spin>();
    feature.component<Bob>();
    feature.system(
        Phase::Update, "cubes::SpinSystem",
        [](Context& ctx) {
            // Runs across worker threads: each entity mutates only its own Transform.
            const float dt = ctx.time.delta;
            ctx.world.parallel_each<Transform, const Spin>(ctx.jobs, [dt](Transform& t, const Spin& s) {
                t.rotation = normalize(Quat::from_axis_angle(s.axis, s.speed * dt) * t.rotation);
            });
        },
        SystemAccess{}.query<Transform, const Spin>());
}

void spawn_grid(Context& ctx, int grid) {
    if (grid <= 0) return;
    std::vector<MaterialHandle> palette(static_cast<std::size_t>(2 * grid - 1));
    for (int x = 0; x < grid; ++x) {
        for (int z = 0; z < grid; ++z) {
            const float fx = static_cast<float>(x - grid / 2) * 1.6f;
            const float fz = static_cast<float>(z - grid / 2) * 1.6f;
            const auto diag = static_cast<std::size_t>(x + z);
            const float hue = static_cast<float>(x + z) / (2.0f * static_cast<float>(grid));
            if (!palette[diag].valid()) {
                palette[diag] = ctx.materials().create_material(
                    {.base_color = {0.9f - hue * 0.6f, 0.4f + hue * 0.4f, 0.3f + hue * 0.6f, 1}});
            }
            ctx.world.create(Transform{.position = {fx, 0, fz}, .scale = {0.8f, 0.8f, 0.8f}},
                             MeshRenderer{ctx.meshes.cube, palette[diag]},
                             Spin{normalize(Vec3{fx, 3, fz}), 0.5f + hue * 2.0f},
                             Bob{.height = 0.4f, .phase = fx * 0.3f + fz * 0.2f});
        }
    }
    ctx.world.create(Transform{.position = {0, -1, 0}, .scale = {60, 1, 60}},
                     MeshRenderer{ctx.meshes.plane, ctx.materials().create_material({.base_color = {0.25f, 0.27f, 0.3f, 1}})});
}

} // namespace cubes
