#include "gallery.hpp"

#include <array>
#include <cstdint>
#include <vector>

using namespace astral;

namespace material_showcase {

namespace {

ImageData make_checker_image(std::uint32_t size, std::uint32_t tiles) {
    std::vector<std::uint8_t> rgba(std::size_t{size} * size * 4, 255);
    const std::uint32_t cell = size / tiles;
    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            const bool light = (((x / cell) + (y / cell)) & 1u) == 0u;
            const std::uint8_t v = light ? 210 : 48;
            const std::size_t i = (std::size_t{y} * size + x) * 4;
            rgba[i] = v;
            rgba[i + 1] = v;
            rgba[i + 2] = static_cast<std::uint8_t>(light ? 216 : 56);
        }
    }
    return {.width = size, .height = size, .rgba = std::move(rgba), .srgb = true};
}

Mat4 box(Vec3 center, Vec3 size) { return translate(center) * scale(size); }

std::array<MaterialDesc, 7> make_materials() {
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

} // namespace

RenderSettings showcase_settings(bool path_tracer) {
    RenderSettings s;
    s.ray_tracing = true;
    s.path_tracer = path_tracer;
    s.path_tracer_max_bounces = 6;
    s.gi = {.probes_x = 8, .probes_y = 4, .probes_z = 6, .rays_per_probe = 64,
            .volume_min = {-7.5f, 0.0f, -4.5f}, .volume_max = {7.5f, 5.0f, 4.5f}};
    s.bloom_strength = 0.04f;
    return s;
}

std::size_t gallery_sphere_count() { return 7; }

void spawn_gallery(Context& ctx) {
    const MeshHandle sphere = ctx.materials().create_mesh(make_sphere(96, 64));
    const MaterialHandle floor_m =
        ctx.materials().create_material({.base_color = {0.20f, 0.21f, 0.23f, 1}, .roughness = 0.58f});
    const MaterialHandle plinth_m =
        ctx.materials().create_material({.base_color = {0.10f, 0.11f, 0.13f, 1}, .roughness = 0.45f});

    TextureHandle checker_tex{};
    if (ctx.renderer) checker_tex = ctx.renderer->create_texture(make_checker_image(256, 8));
    const MaterialHandle wall_m =
        ctx.materials().create_material({.base_color = {0.75f, 0.75f, 0.75f, 1}, .roughness = 0.82f,
                                         .base_color_texture = checker_tex});

    // Studio floor and checkerboard backdrop (makes glass refraction immediately obvious).
    ctx.world.create(WorldTransform{box({0, -0.1f, 0}, {22, 0.2f, 16})}, MeshRenderer{ctx.meshes.cube, floor_m});
    ctx.world.create(WorldTransform{box({0, 3.0f, -3.2f}, {16, 6.2f, 0.2f})}, MeshRenderer{ctx.meshes.cube, wall_m});

    const auto mats = make_materials();
    for (std::size_t i = 0; i < mats.size(); ++i) {
        const float x = (static_cast<float>(i) - 3.0f) * 1.75f;
        const MaterialHandle mh = ctx.materials().create_material(mats[i]);
        ctx.world.create(WorldTransform{box({x, 0.18f, 0}, {1.1f, 0.36f, 1.1f})}, MeshRenderer{ctx.meshes.cube, plinth_m});
        ctx.world.create(WorldTransform{translate({x, 1.06f, 0}) * scale({1.4f, 1.4f, 1.4f})}, MeshRenderer{sphere, mh});
    }

    // Two deferred decals on the studio floor: a wet puddle band + an accent painted stripe.
    ctx.world.create(WorldTransform{box({-1.5f, 0.0f, 1.65f}, {6.2f, 0.5f, 1.3f})},
                     Decal{.color = {0.08f, 0.14f, 0.22f, 0.45f}, .roughness = 0.08f, .wetness = 0.9f, .affect_roughness = true});
    ctx.world.create(WorldTransform{box({3.2f, 0.0f, 1.65f}, {2.2f, 0.5f, 1.1f})},
                     Decal{.color = {0.88f, 0.22f, 0.12f, 0.85f}, .roughness = 0.45f, .affect_roughness = true});

    // Overhead studio softbox + warm rear point light behind the SSS sphere.
    ctx.world.create(RectLight{.position = {0, 4.8f, 1.4f}, .right = {3.6f, 0, 0}, .up = {0, 0, 1.2f},
                               .color = {1.0f, 0.96f, 0.90f}, .intensity = 9.0f});
    ctx.world.create(PointLight{.position = {3.5f, 1.3f, -1.5f}, .color = {1.0f, 0.55f, 0.25f},
                                .intensity = 18.0f, .radius = 8.0f, .source_radius = 0.2f});

    const Vec3 eye{0, 2.45f, 10.8f};
    const Vec3 target{0, 1.0f, 0.0f};
    ctx.world.create(Transform{.position = eye, .rotation = rotation_of(inverse(look_at(eye, target, {0, 1, 0})))},
                     Camera{.fov_y = radians(38.0f), .near_plane = 0.05f});
    ctx.sun = {.direction = {-0.35f, -0.65f, -0.65f}, .color = {1.0f, 0.97f, 0.92f}, .intensity = 1.6f};
    ctx.environment = {.sky = {0.10f, 0.14f, 0.22f}, .ground = {0.04f, 0.04f, 0.05f}, .ambient = 0.35f, .exposure = 1.0f};
}

} // namespace material_showcase
