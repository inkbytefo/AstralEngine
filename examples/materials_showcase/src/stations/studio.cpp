// Studio station (from material_showcase): the 7 M13 PBR shading models, an 8th exhibit bound to
// studio/shaders/car_paint.ashader, 2 deferred floor decals, overhead studio softbox, and backlit SSS point light.
#include "stations/stations.hpp"

#include <array>
#include <string>

using namespace astral;

namespace materials_showcase {

namespace {

Mat4 box(Vec3 center, Vec3 size) { return translate(center) * scale(size); }

std::array<MaterialDesc, studio_pbr_sphere_count> make_studio_materials() {
    return {{
        // 0: Multi-scattering rough gold (energy-conserving at high roughness)
        {.base_color = {0.96f, 0.78f, 0.36f, 1}, .roughness = 0.85f, .metallic = 1.0f},
        // 1: Clearcoat crimson car paint
        {.base_color = {0.62f, 0.03f, 0.05f, 1},
         .roughness = 0.48f,
         .metallic = 0.2f,
         .shading_model = ShadingModel::Clearcoat,
         .clearcoat = {.strength = 1.0f, .roughness = 0.04f}},
        // 2: Royal purple velvet (Charlie sheen)
        {.base_color = {0.07f, 0.02f, 0.14f, 1},
         .roughness = 0.92f,
         .shading_model = ShadingModel::Sheen,
         .sheen = {.color = {0.82f, 0.52f, 1.0f}, .roughness = 0.32f}},
        // 3: Anisotropic brushed copper
        {.base_color = {0.95f, 0.62f, 0.42f, 1},
         .roughness = 0.38f,
         .metallic = 1.0f,
         .shading_model = ShadingModel::Anisotropic,
         .anisotropy = {.strength = 0.88f, .rotation = 0.0f}},
        // 4: Emerald Beer-Lambert transmissive glass
        {.base_color = {0.98f, 0.98f, 0.98f, 1},
         .roughness = 0.03f,
         .shading_model = ShadingModel::Transmission,
         .transmission = {.strength = 1.0f, .ior = 1.52f, .thickness = 1.3f,
                          .attenuation_color = {0.16f, 0.92f, 0.48f}, .attenuation_distance = 1.15f}},
        // 5: Backlit subsurface scattering warm coral wax/jade
        {.base_color = {0.22f, 0.52f, 0.42f, 1},
         .roughness = 0.38f,
         .shading_model = ShadingModel::Subsurface,
         .subsurface = {.strength = 0.95f, .color = {1.0f, 0.32f, 0.12f}, .radius = 0.8f}},
        // 6: Wet porous slate basalt
        {.base_color = {0.42f, 0.40f, 0.38f, 1}, .roughness = 0.82f, .wetness = 0.92f},
    }};
}

float exhibit_x(std::size_t index) { return (static_cast<float>(index) - 3.5f) * 1.65f; }

} // namespace

void build_studio(StationBuild& build) {
    Context& ctx = build.ctx;
    const MeshHandle sphere = build.resources.smooth_sphere(ctx);
    const MaterialHandle floor_m =
        build.material(StationId::Studio, {.base_color = {0.20f, 0.21f, 0.23f, 1}, .roughness = 0.58f});
    const MaterialHandle plinth_m =
        build.material(StationId::Studio, {.base_color = {0.10f, 0.11f, 0.13f, 1}, .roughness = 0.45f});
    const TextureHandle checker_tex = build.resources.checker_texture(ctx);
    const MaterialHandle wall_m =
        build.material(StationId::Studio, {.base_color = {0.75f, 0.75f, 0.75f, 1}, .roughness = 0.82f,
                                           .base_color_texture = checker_tex});

    // Studio floor and checkerboard backdrop (makes glass refraction immediately obvious).
    build.create(WorldTransform{box({0, -0.1f, 0}, {22, 0.2f, 16})}, MeshRenderer{ctx.meshes.cube, floor_m});
    build.create(WorldTransform{box({0, 3.0f, -3.2f}, {16, 6.2f, 0.2f})}, MeshRenderer{ctx.meshes.cube, wall_m});

    const auto mats = make_studio_materials();
    for (std::size_t i = 0; i < mats.size(); ++i) {
        const float x = exhibit_x(i);
        const MaterialHandle mh = build.material(StationId::Studio, mats[i]);
        build.create(WorldTransform{box({x, 0.18f, 0}, {1.05f, 0.36f, 1.05f})}, MeshRenderer{ctx.meshes.cube, plinth_m});
        build.create(WorldTransform{translate({x, 1.035f, 0}) * scale({1.35f, 1.35f, 1.35f})}, MeshRenderer{sphere, mh});
    }

    // 8th exhibit: custom clearcoat .ashader (studio/shaders/car_paint.ashader via car_paint.material).
    const float car_x = exhibit_x(studio_pbr_sphere_count);
    build.create(WorldTransform{box({car_x, 0.18f, 0}, {1.05f, 0.36f, 1.05f})}, MeshRenderer{ctx.meshes.cube, plinth_m});
    build.create(Name{"car_paint_exhibit"}, Transform{.position = {car_x, 1.035f, 0}, .scale = {1.35f, 1.35f, 1.35f}},
                 PrimitiveMesh{.shape = Primitive::Sphere, .material_file = std::string(studio_car_paint_material)});

    // Two deferred decals on the studio floor: a wet puddle band and a painted accent stripe.
    build.create(WorldTransform{box({-1.5f, 0.0f, 1.65f}, {6.2f, 0.5f, 1.3f})},
                 Decal{.color = {0.08f, 0.14f, 0.22f, 0.45f}, .roughness = 0.08f, .wetness = 0.9f, .affect_roughness = true});
    build.create(WorldTransform{box({3.2f, 0.0f, 1.65f}, {2.2f, 0.5f, 1.1f})},
                 Decal{.color = {0.88f, 0.22f, 0.12f, 0.85f}, .roughness = 0.45f, .affect_roughness = true});

    // Overhead studio softbox + warm rear point light behind the SSS sphere (index 5).
    build.create(RectLight{.position = {0, 4.8f, 1.4f}, .right = {3.6f, 0, 0}, .up = {0, 0, 1.2f},
                           .color = {1.0f, 0.96f, 0.90f}, .intensity = 9.0f});
    build.create(PointLight{.position = {exhibit_x(5), 1.3f, -1.5f}, .color = {1.0f, 0.55f, 0.25f},
                            .intensity = 18.0f, .radius = 8.0f, .source_radius = 0.2f});
    if (build.raster) {
        build.create(PointLight{.position = {0, 4.5f, 1.4f}, .color = {1.0f, 0.96f, 0.90f},
                                .intensity = 16.0f, .radius = 14.0f});
    }

    build.camera({0, 2.55f, 11.6f}, {0, 1.0f, 0.0f}, 38.0f);
    ctx.sun = {.direction = {-0.35f, -0.65f, -0.65f}, .color = {1.0f, 0.97f, 0.92f}, .intensity = 1.6f};
    ctx.environment = {.sky = {0.10f, 0.14f, 0.22f}, .ground = {0.04f, 0.04f, 0.05f}, .ambient = 0.35f, .exposure = 1.0f};
}

} // namespace materials_showcase
