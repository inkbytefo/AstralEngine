// The "RGB room" (from rt_showcase): red and green walls, a mirror-chrome sphere on a plinth, glossy and rough boxes,
// a gold SDF torus, melting SDF blobs and a single ceiling panel light. Everything beyond the panel's direct light
// (colour bleeding, soft contact shadows, the room in the sphere) comes from ray tracing.
#include "stations/stations.hpp"

using namespace astral;

namespace lighting_showcase {

namespace {

Mat4 box(Vec3 center, Vec3 size, float yaw_degrees = 0.0f) {
    return translate(center) * rotate(Quat::from_axis_angle({0, 1, 0}, radians(yaw_degrees))) * scale(size);
}

void build_room_meshes(StationBuild& build) {
    const auto place = [&](MeshHandle mesh, const Mat4& transform, Vec4 color, float roughness, float metallic) {
        // Placed by matrix (no Transform: never recomputed).
        build.create(WorldTransform{transform},
                     MeshRenderer{mesh, build.material(StationId::Room,
                                                       {.base_color = color, .roughness = roughness, .metallic = metallic})});
    };
    const MeshHandle cube = build.ctx.meshes.cube;
    // Inner size and wall thickness. The room runs from the back wall (z = -3.5) past the camera's field of view at
    // the open front (z = 5.5), like a photo studio set.
    const float W = 6.0f, H = 5.0f, D = 9.0f, t = 0.2f, zc = 1.0f;
    const Vec4 white{0.78f, 0.78f, 0.76f, 1}, red{0.82f, 0.06f, 0.05f, 1}, green{0.08f, 0.62f, 0.12f, 1},
        blue{0.06f, 0.18f, 0.85f, 1};
    // Shell: floor, ceiling, back wall, red left wall, green right wall.
    place(cube, box({0, -t / 2, zc}, {W + 2 * t, t, D}), white, 0.35f, 0);
    place(cube, box({0, H + t / 2, zc}, {W + 2 * t, t, D}), white, 0.9f, 0);
    place(cube, box({0, H / 2, zc - D / 2 - t / 2}, {W + 2 * t, H, t}), white, 0.9f, 0);
    place(cube, box({-W / 2 - t / 2, H / 2, zc}, {t, H, D}), red, 0.85f, 0);
    place(cube, box({W / 2 + t / 2, H / 2, zc}, {t, H, D}), green, 0.85f, 0);
    // Mirror-chrome sphere in the middle, on a low white plinth.
    place(cube, box({0, 0.25f, -0.4f}, {1.6f, 0.5f, 1.6f}), white, 0.6f, 0);
    place(build.resources.smooth_sphere(build.ctx), translate({0, 1.45f, -0.4f}) * scale({1.9f, 1.9f, 1.9f}),
          {0.97f, 0.97f, 0.97f, 1}, 0.02f, 1.0f);
    // A tall glossy blue box (back left), a short rough white box (front right) with a brushed copper cube on it.
    place(cube, box({-1.75f, 1.6f, -2.0f}, {1.3f, 3.2f, 1.3f}, 22), blue, 0.18f, 0);
    place(cube, box({1.8f, 0.6f, 0.9f}, {1.2f, 1.2f, 1.2f}, -18), white, 0.7f, 0);
    place(cube, box({1.75f, 1.55f, 0.85f}, {0.6f, 0.6f, 0.6f}, 30), {0.95f, 0.64f, 0.54f, 1}, 0.32f, 1.0f);
}

void build_room_shapes(StationBuild& build) {
    const SdfShape shapes[] = {
        // Gold torus leaning on the floor (raymarched, but lit, shadowed and reflected like a mesh).
        {.primitive = SdfPrimitive::Torus, .operation = SdfOperation::Union, .position = {-0.9f, 0.32f, 1.6f},
         .rotation = Quat::from_axis_angle({1, 0, 0.3f}, radians(70)), .size = {0.45f, 0.14f, 0, 0},
         .color = {1.0f, 0.78f, 0.34f, 1}, .roughness = 0.22f, .metallic = 1.0f},
        // Two rubbery blobs melting together (smooth union).
        {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::Union, .position = {0.65f, 0.28f, 2.1f},
         .size = {0.28f, 0, 0, 0}, .color = {0.85f, 0.1f, 0.08f, 1}, .roughness = 0.45f},
        {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::SmoothUnion, .position = {1.05f, 0.22f, 2.25f},
         .size = {0.22f, 0, 0, 0}, .blend = 0.18f, .color = {0.95f, 0.85f, 0.2f, 1}, .roughness = 0.45f},
    };
    std::int32_t order = 0;
    for (const SdfShape& s : shapes) {
        build.create(Transform{.position = s.position, .rotation = s.rotation}, Sdf{.shape = s, .order = order++});
    }
}

} // namespace

void build_room(StationBuild& build) {
    build_room_meshes(build);
    build_room_shapes(build);
    // Ceiling panel light, shining down (cross(right, up) = -y).
    build.create(RectLight{.position = {0, 4.99f, -0.3f}, .right = {0.85f, 0, 0}, .up = {0, 0, 0.85f},
                           .color = {1.0f, 0.93f, 0.84f}, .intensity = 18.0f});
    if (build.raster) { // the raster path does not draw rect lights
        build.create(PointLight{.position = {0, 4.4f, 2.0f}, .color = {1.0f, 0.93f, 0.84f}, .intensity = 12.0f, .radius = 14.0f});
    }
    build.camera({0, 2.45f, 9.6f}, {0, 2.2f, 0}, 40);
    build.ctx.sun = {.intensity = 0.0f};
    build.ctx.environment = {.sky = {0.02f, 0.025f, 0.03f}, .ground = {0.01f, 0.01f, 0.01f}, .exposure = 1.1f};
}

} // namespace lighting_showcase
