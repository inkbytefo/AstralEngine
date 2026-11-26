// The SDF yard (from sdf_scene): blobs melt into each other (smooth union), a box is carved by a sphere (subtract), a
// metal torus tumbles, coloured point lights orbit, and the sun casts shadows from both meshes and SDFs.
#include "features/orbits/orbits.hpp"
#include "stations/stations.hpp"

#include <cmath>

using namespace astral;

namespace lighting_showcase {

namespace {

void build_yard_shapes(StationBuild& build) {
    // Three blobs that melt together while orbiting.
    const Vec4 blob_colors[3] = {{0.9f, 0.25f, 0.2f, 1}, {0.2f, 0.6f, 0.95f, 1}, {0.95f, 0.85f, 0.25f, 1}};
    for (int i = 0; i < 3; ++i) {
        build.create(Transform{},
                     orbits::Orbit{.radius = 0.9f, .speed = 0.7f + 0.2f * static_cast<float>(i), .height = 1.6f,
                                   .phase = static_cast<float>(i) * 2.1f},
                     Sdf{.shape = {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::SmoothUnion,
                                   .size = {0.75f, 0, 0, 0}, .blend = 0.9f, .color = blob_colors[i], .roughness = 0.3f},
                         .order = 0});
    }
    // A rounded box carved by a sphere.
    build.create(Transform{.position = {-3.5f, 1.0f, 2.5f}},
                 Sdf{.shape = {.primitive = SdfPrimitive::Box, .operation = SdfOperation::Union,
                               .size = {1, 1, 1, 0.12f}, .color = {0.85f, 0.85f, 0.9f, 1}, .roughness = 0.5f},
                     .order = 10});
    build.create(Transform{.position = {-3.5f, 1.6f, 3.2f}},
                 Sdf{.shape = {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::SmoothSubtract,
                               .size = {0.9f, 0, 0, 0}, .blend = 0.1f},
                     .order = 11});
    // A tumbling metal torus.
    build.create(Transform{.position = {3.5f, 1.4f, 2.0f}},
                 orbits::Tumble{.axis = normalize(Vec3{1, 0.3f, 0}), .speed = 0.8f},
                 Sdf{.shape = {.primitive = SdfPrimitive::Torus, .operation = SdfOperation::Union, .size = {0.9f, 0.3f, 0, 0},
                               .color = {0.9f, 0.9f, 0.95f, 1}, .roughness = 0.15f, .metallic = 1.0f},
                     .order = 20});
}

} // namespace

void build_yard(StationBuild& build) {
    const MaterialHandle ground = build.material(StationId::Yard, {.base_color = {0.35f, 0.36f, 0.4f, 1}, .roughness = 0.8f});
    const MaterialHandle gold = build.material(StationId::Yard,
                                               {.base_color = {0.95f, 0.75f, 0.35f, 1}, .roughness = 0.25f, .metallic = 1.0f});
    build.create(Transform{.scale = {40, 1, 40}}, MeshRenderer{build.ctx.meshes.plane, ground});
    for (int i = 0; i < 5; ++i) {
        const float a = static_cast<float>(i) * 1.2566f;
        build.create(Transform{.position = {std::cos(a) * 6.5f, 0.6f, std::sin(a) * 6.5f}, .scale = {1.2f, 1.2f, 1.2f}},
                     MeshRenderer{build.ctx.meshes.sphere, gold});
    }
    build_yard_shapes(build);

    const Vec3 light_colors[3] = {{1, 0.3f, 0.2f}, {0.2f, 0.5f, 1}, {0.3f, 1, 0.4f}};
    for (int i = 0; i < 3; ++i) {
        build.create(Transform{},
                     orbits::Orbit{.radius = 5.0f, .speed = 0.5f, .height = 2.5f, .phase = static_cast<float>(i) * 2.094f},
                     PointLight{.color = light_colors[i], .intensity = 25.0f, .radius = 9.0f});
    }
    build.camera({0, 6, 11}, {0, 0.7f, 0}, 60);
    build.ctx.sun = {.direction = {-0.5f, -1.0f, -0.35f}, .intensity = 1.6f};
    build.ctx.environment = {.ambient = 0.35f, .exposure = 0.55f};
}

} // namespace lighting_showcase
