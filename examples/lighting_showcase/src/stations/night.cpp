// Many lights: a 6 x 6 grid of coloured point lights over spheres and pillars. With this many local lights the
// ray-traced path picks lights per pixel; ReSTIR DI reuses good picks across pixels and frames (R toggles it).
#include "stations/stations.hpp"

#include <cmath>

using namespace astral;

namespace lighting_showcase {

namespace {

constexpr int grid = 6;
static_assert(grid * grid == static_cast<int>(night_light_count));
constexpr float spacing = 3.0f;

/// A saturated colour around the hue circle, `t` in turns.
Vec3 hue(float t) {
    const auto channel = [&](float shift) { return 0.5f + 0.5f * std::cos(6.2831853f * (t + shift)); };
    return {0.1f + 0.9f * channel(0.0f), 0.1f + 0.9f * channel(0.67f), 0.1f + 0.9f * channel(0.33f)};
}

void build_night_lights(StationBuild& build) {
    for (int z = 0; z < grid; ++z) {
        for (int x = 0; x < grid; ++x) {
            const int i = z * grid + x;
            const Vec3 at{(static_cast<float>(x) - 2.5f) * spacing, 0.7f + 0.3f * static_cast<float>(i % 3),
                          (static_cast<float>(z) - 2.5f) * spacing};
            build.create(PointLight{.position = at, .color = hue(static_cast<float>(i) * 0.381966f), .intensity = 5.0f,
                                    .radius = 4.5f, .source_radius = 0.08f});
        }
    }
}

} // namespace

void build_night(StationBuild& build) {
    const MaterialHandle floor = build.material(StationId::Night, {.base_color = {0.22f, 0.23f, 0.26f, 1}, .roughness = 0.4f});
    const MaterialHandle chrome = build.material(StationId::Night,
                                                 {.base_color = {0.92f, 0.92f, 0.94f, 1}, .roughness = 0.08f, .metallic = 1.0f});
    const MaterialHandle plaster = build.material(StationId::Night, {.base_color = {0.8f, 0.78f, 0.75f, 1}, .roughness = 0.85f});
    build.create(Transform{.scale = {30, 1, 30}}, MeshRenderer{build.ctx.meshes.plane, floor});
    // Between the lights: spheres and pillars that cast many coloured shadows.
    for (int z = 0; z < grid - 1; ++z) {
        for (int x = 0; x < grid - 1; ++x) {
            const float px = (static_cast<float>(x) - 2.0f) * spacing, pz = (static_cast<float>(z) - 2.0f) * spacing;
            if ((x + z) % 2 == 0) {
                build.create(Transform{.position = {px, 0.55f, pz}, .scale = {1.1f, 1.1f, 1.1f}},
                             MeshRenderer{build.resources.smooth_sphere(build.ctx), chrome});
            } else {
                const float height = 1.2f + 0.4f * static_cast<float>((x * 3 + z) % 3);
                build.create(Transform{.position = {px, height * 0.5f, pz}, .scale = {0.55f, height, 0.55f}},
                             MeshRenderer{build.ctx.meshes.cube, plaster});
            }
        }
    }
    build_night_lights(build);
    build.camera({0, 7.5f, 15.5f}, {0, 0.2f, 0}, 48);
    build.ctx.sun = {.direction = {-0.3f, -1.0f, -0.2f}, .color = {0.55f, 0.65f, 1.0f}, .intensity = 0.05f};
    build.ctx.environment = {.sky = {0.012f, 0.016f, 0.035f}, .ground = {0.006f, 0.006f, 0.008f}, .ambient = 0.3f, .exposure = 1.0f};
}

} // namespace lighting_showcase
