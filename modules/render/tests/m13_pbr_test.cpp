// M13 Advanced PBR, Deferred Decals, and Progressive Path Tracer regression & math tests (ADR-0033).
#include "gpu_test_support.hpp"

#include <astral/core/math.hpp>
#include <astral/render/mesh.hpp>
#include <astral/render/renderer.hpp>
#include <astral/render/surface_shader.hpp>
#include <astral/shader/ashader.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <vector>

using namespace astral;

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

Vec2 env_brdf_ab(float roughness, float nv) {
    const Vec4 c0{-1.0f, -0.0275f, -0.572f, 0.022f};
    const Vec4 c1{1.0f, 0.0425f, 1.04f, -0.04f};
    const Vec4 r{roughness * c0.x + c1.x, roughness * c0.y + c1.y, roughness * c0.z + c1.z, roughness * c0.w + c1.w};
    const float a004 = std::min(r.x * r.x, std::exp2(-9.28f * nv)) * r.x + r.y;
    return {-1.04f * a004 + r.z, 1.04f * a004 + r.w};
}

Vec3 ggx_ms_factor(Vec3 f0, float roughness, float nv) {
    const Vec2 ab = env_brdf_ab(roughness, nv);
    const float ess = std::clamp(ab.x + ab.y, 0.05f, 1.0f);
    const float ems = 1.0f - ess;
    const float scale = ems / ess;
    return {1.0f + f0.x * scale, 1.0f + f0.y * scale, 1.0f + f0.z * scale};
}

float charlie_d(float nh, float roughness) {
    const float r = std::clamp(roughness, 0.05f, 1.0f);
    const float inv_r = 1.0f / (r * r);
    const float sin2h = std::max(1.0f - nh * nh, 1e-4f);
    return (2.0f + inv_r) * std::pow(sin2h, inv_r * 0.5f) / (2.0f * kPi);
}

void aniso_alpha(float roughness, float anisotropy, float& ax, float& ay) {
    const float a = std::max(roughness * roughness, 0.002f);
    const float k = std::clamp(anisotropy, -0.95f, 0.95f);
    const float aspect = std::sqrt(std::max(1.0f - 0.9f * std::abs(k), 0.05f));
    ax = k >= 0.0f ? std::max(a / aspect, 0.002f) : std::max(a * aspect, 0.002f);
    ay = k >= 0.0f ? std::max(a * aspect, 0.002f) : std::max(a / aspect, 0.002f);
}

float fresnel_dielectric(float cos_i, float eta) {
    const float c = std::clamp(cos_i, 0.0f, 1.0f);
    const float sin2_t = eta * eta * std::max(0.0f, 1.0f - c * c);
    if (sin2_t >= 1.0f) return 1.0f;
    const float cos_t = std::sqrt(1.0f - sin2_t);
    const float rs = (eta * c - cos_t) / std::max(eta * c + cos_t, 1e-6f);
    const float rp = (c - eta * cos_t) / std::max(c + eta * cos_t, 1e-6f);
    return 0.5f * (rs * rs + rp * rp);
}

Vec3 beer_lambert(Vec3 att_color, float att_dist, float dist) {
    const float d = std::max(att_dist, 1e-4f);
    auto ch = [&](float c) { return std::exp(std::log(std::clamp(c, 1e-4f, 1.0f)) * (std::max(dist, 0.0f) / d)); };
    return {ch(att_color.x), ch(att_color.y), ch(att_color.z)};
}

struct PbrRig {
    static constexpr std::uint32_t kSize = 64;
    test::TestDevice device;
    std::unique_ptr<Renderer> renderer;
    gpu::TextureHandle target;
    gpu::BufferHandle readback;
    CameraData camera;

    explicit PbrRig(RenderSettings settings = {}) {
        if (!device) return;
        renderer = std::make_unique<Renderer>(*device, gpu::Format::RGBA8Unorm, settings);
        target = device->create_texture({.width = kSize, .height = kSize, .format = gpu::Format::RGBA8Unorm,
                                         .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::TransferSrc});
        readback = device->create_buffer({.size = kSize * kSize * 4, .usage = gpu::BufferUsage::TransferDst,
                                          .memory = gpu::MemoryUsage::GpuToCpu});
        look_from({0, 0, 4.0f}, {0, 0, 0});
    }

    void look_from(Vec3 eye, Vec3 at) {
        camera.position = eye;
        const Vec3 up = std::abs(eye.y - at.y) > 0.1f && eye.x == at.x && eye.z == at.z ? Vec3{0, 0, -1} : Vec3{0, 1, 0};
        camera.view = look_at(eye, at, up);
        camera.projection = perspective(radians(50.0f), 1.0f, 0.1f, 100.0f);
    }

    std::vector<std::uint32_t> draw(const RenderScene& scene, int frames = 1) {
        for (int i = 0; i < frames; ++i) {
            auto cmd = device->begin_commands();
            renderer->render(cmd, target, camera, scene);
            if (i + 1 == frames) {
                cmd.transition(target, gpu::ResourceState::TransferSrc);
                cmd.copy_texture_to_buffer(target, readback);
            }
            device->wait(device->submit(cmd));
        }
        std::vector<std::uint32_t> pixels(kSize * kSize);
        device->read_buffer(readback, pixels.data(), pixels.size() * 4);
        return pixels;
    }

    static std::array<int, 3> rgb(std::uint32_t p) {
        return {static_cast<int>(p & 0xFF), static_cast<int>((p >> 8) & 0xFF), static_cast<int>((p >> 16) & 0xFF)};
    }
    static float luma(std::uint32_t p) {
        const auto c = rgb(p);
        return (c[0] + c[1] + c[2]) / 3.0f;
    }
    static float mean_luma(const std::vector<std::uint32_t>& px) {
        double sum = 0.0;
        for (std::uint32_t p : px) sum += luma(p);
        return static_cast<float>(sum / static_cast<double>(px.size()));
    }
    std::uint32_t at(const std::vector<std::uint32_t>& px, Vec3 world) const {
        const Vec4 clip = camera.projection * camera.view * Vec4{world.x, world.y, world.z, 1};
        const auto ix = static_cast<std::uint32_t>(std::clamp((clip.x / clip.w * 0.5f + 0.5f) * kSize, 0.0f, kSize - 1.0f));
        const auto iy = static_cast<std::uint32_t>(std::clamp((clip.y / clip.w * 0.5f + 0.5f) * kSize, 0.0f, kSize - 1.0f));
        return px[iy * kSize + ix];
    }
};

RenderSettings rt_settings(bool pt = false) {
    RenderSettings s;
    s.ray_tracing = true;
    s.path_tracer = pt;
    s.path_tracer_max_bounces = 4;
    s.gi = {.probes_x = 4, .probes_y = 4, .probes_z = 4, .rays_per_probe = 32};
    return s;
}

} // namespace

// --- CPU Mathematical Verification ----------------------------------------------------------

TEST_CASE("math contract: Turquin multi-scattering GGX compensates white-furnace energy loss") {
    for (float r : {0.1f, 0.4f, 0.7f, 1.0f}) {
        for (float nv : {0.25f, 0.6f, 1.0f}) {
            const Vec2 ab = env_brdf_ab(r, nv);
            const float ess = std::clamp(ab.x + ab.y, 0.05f, 1.0f);
            const Vec3 ms = ggx_ms_factor({1.0f, 1.0f, 1.0f}, r, nv);
            const float multi_scatter_furnace = ess * ms.x;
            CHECK(multi_scatter_furnace == doctest::Approx(1.0f).epsilon(1e-4));
            CHECK(ms.x >= 1.0f);
        }
    }
    // Compensation factor grows with roughness because single-scattering loses more energy on rough metals.
    CHECK(ggx_ms_factor({1, 1, 1}, 0.9f, 0.5f).x > ggx_ms_factor({1, 1, 1}, 0.2f, 0.5f).x);
}

TEST_CASE("math contract: Charlie sheen, anisotropic GGX, dielectric Fresnel, and Beer-Lambert bounds") {
    // Charlie sheen peaks at grazing (small N.H -> sin(theta) near 1) and stays strictly positive.
    CHECK(charlie_d(0.2f, 0.4f) > charlie_d(0.95f, 0.4f));
    CHECK(charlie_d(0.5f, 0.3f) > 0.0f);

    float ax_p = 0, ay_p = 0, ax_n = 0, ay_n = 0;
    aniso_alpha(0.5f, 0.8f, ax_p, ay_p);
    aniso_alpha(0.5f, -0.8f, ax_n, ay_n);
    CHECK(ax_p > ay_p * 1.5f);
    CHECK(ay_n > ax_n * 1.5f);
    CHECK(ax_p == doctest::Approx(ay_n).epsilon(1e-5));

    // Glass IOR = 1.5 -> eta = 1/1.5: normal incidence Fresnel = ((0.5)/(2.5))^2 = 0.04; grazing -> 1.0; TIR -> 1.0.
    CHECK(fresnel_dielectric(1.0f, 1.0f / 1.5f) == doctest::Approx(0.04f).epsilon(1e-4));
    CHECK(fresnel_dielectric(0.01f, 1.0f / 1.5f) > 0.9f);
    CHECK(fresnel_dielectric(0.5f, 1.5f) == doctest::Approx(1.0f)); // sin^2 = 1.5^2 * 0.75 > 1 -> TIR

    const Vec3 tint{0.2f, 0.8f, 0.4f};
    const Vec3 b0 = beer_lambert(tint, 2.0f, 0.0f);
    const Vec3 b1 = beer_lambert(tint, 2.0f, 2.0f);
    const Vec3 b2 = beer_lambert(tint, 2.0f, 4.0f);
    CHECK(b0.y == doctest::Approx(1.0f));
    CHECK(b1.x == doctest::Approx(tint.x).epsilon(1e-4));
    CHECK(b1.y == doctest::Approx(tint.y).epsilon(1e-4));
    CHECK(b2.y < b1.y);
}

// --- GPU Regression Tests -------------------------------------------------------------------

TEST_CASE("M13 PBR: clearcoat adds sharp glossy highlight over rough base across raster, RT, and PT") {
    PbrRig rig(rt_settings(false));
    if (!rig.device) return;
    const bool has_rt = rig.renderer->ray_tracing();
    const MeshHandle sphere = rig.renderer->create_mesh(make_sphere(32, 16));
    const MaterialHandle rough = rig.renderer->create_material({.base_color = {0.04f, 0.06f, 0.12f, 1}, .roughness = 0.85f});
    const MaterialHandle coated = rig.renderer->create_material(
        {.base_color = {0.04f, 0.06f, 0.12f, 1},
         .roughness = 0.85f,
         .shading_model = ShadingModel::Clearcoat,
         .clearcoat = {.strength = 1.0f, .roughness = 0.06f}});
    const std::array a_rough{MeshInstance{sphere, rough, scale({2, 2, 2})}};
    const std::array a_coat{MeshInstance{sphere, coated, scale({2, 2, 2})}};
    const RenderScene s_rough{.meshes = a_rough,
                              .sun = {.direction = {0, 0, -1}, .color = {1, 1, 1}, .intensity = 2.5f, .shadows = false},
                              .environment = {.sky = {0.01f, 0.01f, 0.01f}, .ground = {0.01f, 0.01f, 0.01f}, .ambient = 0.05f}};
    const RenderScene s_coat{.meshes = a_coat,
                             .sun = {.direction = {0, 0, -1}, .color = {1, 1, 1}, .intensity = 2.5f, .shadows = false},
                             .environment = {.sky = {0.01f, 0.01f, 0.01f}, .ground = {0.01f, 0.01f, 0.01f}, .ambient = 0.05f}};
    for (int mode = 0; mode < (has_rt ? 3 : 1); ++mode) {
        rig.renderer->set_settings(mode == 0 ? RenderSettings{.ray_tracing = false} : rt_settings(mode == 2));
        const int n = mode == 2 ? 12 : (mode == 1 ? 6 : 1);
        const auto px_rough = rig.draw(s_rough, n);
        rig.renderer->reset_history();
        const auto px_coat = rig.draw(s_coat, n);
        const float l_rough = PbrRig::luma(rig.at(px_rough, {0, 0, 0.99f}));
        const float l_coat = PbrRig::luma(rig.at(px_coat, {0, 0, 0.99f}));
        CHECK_MESSAGE(l_coat > l_rough + 40.0f, "mode=", mode, " rough=", l_rough, " coat=", l_coat);
    }
}

TEST_CASE("M13 PBR: Charlie sheen adds grazing rim highlight across raster, RT, and PT") {
    PbrRig rig(rt_settings(false));
    if (!rig.device) return;
    const bool has_rt = rig.renderer->ray_tracing();
    const MeshHandle sphere = rig.renderer->create_mesh(make_sphere(32, 16));
    const MaterialHandle base = rig.renderer->create_material({.base_color = {0.03f, 0.03f, 0.03f, 1}, .roughness = 0.9f});
    const MaterialHandle velvet = rig.renderer->create_material(
        {.base_color = {0.03f, 0.03f, 0.03f, 1},
         .roughness = 0.9f,
         .shading_model = ShadingModel::Sheen,
         .sheen = {.color = {1.0f, 0.25f, 0.45f}, .roughness = 0.35f}});
    const std::array items{MeshInstance{sphere, base, translate({-1.1f, 0, 0}) * scale({2, 2, 2})},
                           MeshInstance{sphere, velvet, translate({1.1f, 0, 0}) * scale({2, 2, 2})}};
    const RenderScene scene{.meshes = items,
                            .sun = {.direction = {0, -0.3f, -1}, .color = {1, 1, 1}, .intensity = 6.0f, .shadows = false},
                            .environment = {.sky = {0.2f, 0.2f, 0.2f}, .ground = {0.1f, 0.1f, 0.1f}, .ambient = 0.3f}};
    for (int mode = 0; mode < (has_rt ? 3 : 1); ++mode) {
        rig.renderer->set_settings(mode == 0 ? RenderSettings{.ray_tracing = false} : rt_settings(mode == 2));
        const auto px = rig.draw(scene, mode == 2 ? 12 : (mode == 1 ? 6 : 1));
        const auto rim_base = PbrRig::rgb(rig.at(px, {-1.1f + 0.88f, 0.0f, 0.45f}));
        const auto rim_velvet = PbrRig::rgb(rig.at(px, {1.1f + 0.88f, 0.0f, 0.45f}));
        CHECK_MESSAGE(rim_velvet[0] > rim_base[0] + 15, "mode=", mode, " baseR=", rim_base[0], " velvetR=", rim_velvet[0]);
    }
}

TEST_CASE("M13 PBR: anisotropic brushed metal shifts specular lobe along tangent vs bitangent") {
    PbrRig rig(rt_settings(false));
    if (!rig.device) return;
    const bool has_rt = rig.renderer->ray_tracing();
    const MeshHandle sphere = rig.renderer->create_mesh(make_sphere(32, 16));
    const MaterialHandle aniso_h = rig.renderer->create_material(
        {.base_color = {0.95f, 0.78f, 0.35f, 1},
         .roughness = 0.42f,
         .metallic = 1.0f,
         .shading_model = ShadingModel::Anisotropic,
         .anisotropy = {.strength = 0.88f, .rotation = 0.0f}});
    const MaterialHandle aniso_v = rig.renderer->create_material(
        {.base_color = {0.95f, 0.78f, 0.35f, 1},
         .roughness = 0.42f,
         .metallic = 1.0f,
         .shading_model = ShadingModel::Anisotropic,
         .anisotropy = {.strength = -0.88f, .rotation = 0.0f}});
    const std::array items{MeshInstance{sphere, aniso_h, translate({-1.1f, 0, 0}) * scale({2, 2, 2})},
                           MeshInstance{sphere, aniso_v, translate({1.1f, 0, 0}) * scale({2, 2, 2})}};
    const RenderScene scene{.meshes = items,
                            .sun = {.direction = {0, 0, -1}, .color = {1, 1, 1}, .intensity = 8.0f, .shadows = false},
                            .environment = {.sky = {0.02f, 0.02f, 0.02f}, .ground = {0.01f, 0.01f, 0.01f}, .ambient = 0.05f}};
    for (int mode = 0; mode < (has_rt ? 2 : 1); ++mode) {
        rig.renderer->set_settings(mode == 0 ? RenderSettings{.ray_tracing = false} : rt_settings(false));
        const auto px = rig.draw(scene, mode == 1 ? 6 : 1);
        const float h_x = PbrRig::luma(rig.at(px, {-1.1f + 0.52f, 0.0f, 0.85f}));
        const float h_y = PbrRig::luma(rig.at(px, {-1.1f, 0.52f, 0.85f}));
        const float v_x = PbrRig::luma(rig.at(px, {1.1f + 0.52f, 0.0f, 0.85f}));
        const float v_y = PbrRig::luma(rig.at(px, {1.1f, 0.52f, 0.85f}));
        CHECK_MESSAGE((h_x - h_y) * (v_x - v_y) < 0.0f, "rt=", mode, " hx=", h_x, " hy=", h_y, " vx=", v_x, " vy=", v_y);
    }
}

TEST_CASE("M13 PBR: transmission refracts backdrop with Beer-Lambert tint in RT and path tracer") {
    PbrRig rig(rt_settings(false));
    if (!rig.device || !rig.renderer->ray_tracing()) return;
    const MeshHandle sphere = rig.renderer->create_mesh(make_sphere(32, 16));
    const MeshHandle quad = rig.renderer->create_mesh(make_cube());
    const MaterialHandle backdrop = rig.renderer->create_material(
        {.base_color = {0, 0, 0, 1}, .roughness = 1.0f, .emissive = {3.5f, 3.5f, 3.5f}});
    const MaterialHandle glass = rig.renderer->create_material(
        {.base_color = {0.95f, 0.95f, 0.95f, 1},
         .roughness = 0.03f,
         .shading_model = ShadingModel::Transmission,
         .transmission = {.strength = 1.0f, .ior = 1.5f, .thickness = 1.8f,
                          .attenuation_color = {0.15f, 0.95f, 0.25f}, .attenuation_distance = 1.5f}});
    const std::array items{
        MeshInstance{quad, backdrop, translate({0, 0, -4.0f}) * scale({10, 10, 0.2f})},
        MeshInstance{sphere, glass, scale({2, 2, 2})},
    };
    const RenderScene scene{.meshes = items, .sun = {.intensity = 0.0f},
                            .environment = {.sky = {0, 0, 0}, .ground = {0, 0, 0}, .ambient = 0.0f}};
    for (bool pt : {false, true}) {
        rig.renderer->set_settings(rt_settings(pt));
        const auto px = rig.draw(scene, pt ? 16 : 8);
        const auto c = PbrRig::rgb(rig.at(px, {0, 0, 0}));
        CHECK_MESSAGE(c[1] > 80, "pt=", pt, " rgb=(", c[0], ",", c[1], ",", c[2], ")");
        CHECK_MESSAGE(c[1] > c[0] + 20, "pt=", pt, " rgb=(", c[0], ",", c[1], ",", c[2], ")");
    }
}

TEST_CASE("M13 PBR: subsurface scattering transmits warm light onto backlit hemisphere") {
    PbrRig rig(rt_settings(false));
    if (!rig.device) return;
    const bool has_rt = rig.renderer->ray_tracing();
    const MeshHandle sphere = rig.renderer->create_mesh(make_sphere(32, 16));
    const MaterialHandle opaque = rig.renderer->create_material({.base_color = {0.5f, 0.5f, 0.5f, 1}, .roughness = 0.7f});
    const MaterialHandle sss = rig.renderer->create_material(
        {.base_color = {0.5f, 0.5f, 0.5f, 1},
         .roughness = 0.5f,
         .shading_model = ShadingModel::Subsurface,
         .subsurface = {.strength = 1.0f, .color = {1.0f, 0.28f, 0.1f}, .radius = 0.8f}});
    const std::array items{MeshInstance{sphere, opaque, translate({-1.1f, 0, 0}) * scale({2, 2, 2})},
                           MeshInstance{sphere, sss, translate({1.1f, 0, 0}) * scale({2, 2, 2})}};
    const RenderScene scene{.meshes = items,
                            .sun = {.direction = {0, 0, 1}, .color = {1, 1, 1}, .intensity = 8.0f, .shadows = false},
                            .environment = {.sky = {0, 0, 0}, .ground = {0, 0, 0}, .ambient = 0.0f}};
    for (int mode = 0; mode < (has_rt ? 3 : 1); ++mode) {
        rig.renderer->set_settings(mode == 0 ? RenderSettings{.ray_tracing = false} : rt_settings(mode == 2));
        const auto px = rig.draw(scene, mode == 2 ? 12 : (mode == 1 ? 6 : 1));
        const auto c_opaque = PbrRig::rgb(rig.at(px, {-1.1f, 0, 0.99f}));
        const auto c_sss = PbrRig::rgb(rig.at(px, {1.1f, 0, 0.99f}));
        CHECK_MESSAGE(c_sss[0] > c_opaque[0] + 25, "mode=", mode, " opaqueR=", c_opaque[0], " sssR=", c_sss[0]);
        CHECK_MESSAGE(c_sss[0] > c_sss[2] + 15, "mode=", mode, " sss=(", c_sss[0], ",", c_sss[1], ",", c_sss[2], ")");
    }
}

TEST_CASE("M13 PBR: wetness darkens porous rough dielectric and deferred decal projects onto floor") {
    PbrRig rig(rt_settings(false));
    if (!rig.device) return;
    const bool has_rt = rig.renderer->ray_tracing();
    rig.look_from({0, 4.0f, 0.01f}, {0, 0, 0});
    const MeshHandle plane = rig.renderer->create_mesh(make_plane(8.0f));
    const MaterialHandle dry = rig.renderer->create_material({.base_color = {0.8f, 0.8f, 0.8f, 1}, .roughness = 0.85f});
    const MaterialHandle wet = rig.renderer->create_material(
        {.base_color = {0.8f, 0.8f, 0.8f, 1}, .roughness = 0.85f, .wetness = 0.95f});
    const std::array items{
        MeshInstance{plane, dry, translate({-2.0f, 0, 0}) * scale({0.45f, 1, 1})},
        MeshInstance{plane, wet, translate({2.0f, 0, 0}) * scale({0.45f, 1, 1})},
    };
    const std::array decals{Decal{
        .transform = translate({-2.0f, 0, 0}) * scale({1.4f, 1.0f, 1.4f}),
        .color = {0.95f, 0.05f, 0.05f, 1.0f},
        .roughness = 0.35f,
        .wetness = 0.2f,
    }};
    const RenderScene scene{.meshes = items,
                            .decals = decals,
                            .sun = {.direction = {0.5f, -0.8f, 0.3f}, .color = {1, 1, 1}, .intensity = 2.5f, .shadows = false},
                            .environment = {.sky = {0.2f, 0.2f, 0.2f}, .ground = {0.1f, 0.1f, 0.1f}, .ambient = 0.25f}};
    for (int mode = 0; mode < (has_rt ? 3 : 1); ++mode) {
        rig.renderer->set_settings(mode == 0 ? RenderSettings{.ray_tracing = false} : rt_settings(mode == 2));
        const auto px = rig.draw(scene, mode == 2 ? 12 : (mode == 1 ? 6 : 1));
        CHECK(rig.renderer->stats().decals == 1u);
        const auto c_decal = PbrRig::rgb(rig.at(px, {-2.0f, 0, 0}));
        const auto c_dry_edge = PbrRig::rgb(rig.at(px, {-2.0f, 0, 1.4f}));
        const auto c_wet = PbrRig::rgb(rig.at(px, {2.0f, 0, 1.4f}));
        CHECK_MESSAGE(c_decal[0] > c_decal[1] + 40, "mode=", mode, " decal=(", c_decal[0], ",", c_decal[1], ",", c_decal[2], ")");
        CHECK_MESSAGE(c_wet[1] + 10 < c_dry_edge[1], "mode=", mode, " wetG=", c_wet[1], " dryG=", c_dry_edge[1]);
    }
}

TEST_CASE("M13 PBR: detail normal and POM height map modulate surface appearance") {
    PbrRig rig(RenderSettings{.ray_tracing = false});
    if (!rig.device) return;
    rig.look_from({1.2f, 1.4f, 2.2f}, {0, 0, 0});
    std::vector<std::uint8_t> stripe_albedo(64 * 64 * 4, 255);
    std::vector<std::uint8_t> height_data(64 * 64 * 4, 255);
    std::vector<std::uint8_t> detail_n(64 * 64 * 4, 255);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * 64 + x) * 4;
            const std::uint8_t stripe = ((x / 8) % 2 == 0) ? 230 : 40;
            stripe_albedo[i] = stripe; stripe_albedo[i + 1] = stripe; stripe_albedo[i + 2] = stripe;
            const std::uint8_t h = static_cast<std::uint8_t>(x * 4);
            height_data[i] = h; height_data[i + 1] = h; height_data[i + 2] = h;
            detail_n[i] = 220; detail_n[i + 1] = 128; detail_n[i + 2] = 180;
        }
    }
    const TextureHandle t_albedo = rig.renderer->create_texture({.width = 64, .height = 64, .rgba = stripe_albedo, .srgb = true});
    const TextureHandle t_height = rig.renderer->create_texture({.width = 64, .height = 64, .rgba = height_data, .srgb = false});
    const TextureHandle t_dnorm = rig.renderer->create_texture({.width = 64, .height = 64, .rgba = detail_n, .srgb = false});
    const MeshHandle plane = rig.renderer->create_mesh(make_plane(4.0f));
    const MaterialHandle plain = rig.renderer->create_material({.roughness = 0.6f, .base_color_texture = t_albedo});
    const MaterialHandle pom = rig.renderer->create_material(
        {.roughness = 0.6f,
         .base_color_texture = t_albedo,
         .detail = {.normal_texture = t_dnorm, .height_texture = t_height, .uv_scale = 2.0f, .normal_scale = 1.0f, .parallax_scale = 0.12f}});
    const std::array a_plain{MeshInstance{plane, plain, Mat4::identity()}};
    const std::array a_pom{MeshInstance{plane, pom, Mat4::identity()}};
    const RenderScene s_plain{.meshes = a_plain, .sun = {.direction = {-0.7f, -0.6f, -0.3f}, .intensity = 3.0f, .shadows = false}};
    const RenderScene s_pom{.meshes = a_pom, .sun = {.direction = {-0.7f, -0.6f, -0.3f}, .intensity = 3.0f, .shadows = false}};
    const auto p0 = rig.draw(s_plain, 1);
    const auto p1 = rig.draw(s_pom, 1);
    int diff_pixels = 0;
    for (std::size_t i = 0; i < p0.size(); ++i) {
        if (std::abs(PbrRig::luma(p0[i]) - PbrRig::luma(p1[i])) > 12.0f) ++diff_pixels;
    }
    CHECK(diff_pixels > 200);
}

TEST_CASE("M13 PBR: path tracer accumulates, resets on camera move, and matches hybrid RT luminance") {
    PbrRig rig(rt_settings(true));
    if (!rig.device || !rig.renderer->ray_tracing()) return;
    rig.look_from({0, 1.5f, 4.5f}, {0, 1.0f, 0});
    const MeshHandle plane = rig.renderer->create_mesh(make_plane(10.0f));
    const MeshHandle cube = rig.renderer->create_mesh(make_cube());
    const MaterialHandle floor_m = rig.renderer->create_material({.base_color = {0.75f, 0.75f, 0.72f, 1}, .roughness = 0.8f});
    const MaterialHandle box_m = rig.renderer->create_material({.base_color = {0.82f, 0.22f, 0.18f, 1}, .roughness = 0.55f});
    const std::array items{
        MeshInstance{plane, floor_m, Mat4::identity()},
        MeshInstance{cube, box_m, translate({0, 0.6f, 0}) * scale({1.2f, 1.2f, 1.2f})},
    };
    const std::array rects{RectLight{.position = {0, 3.2f, 0.5f}, .right = {0.8f, 0, 0}, .up = {0, 0, 0.8f}, .intensity = 10.0f}};
    const RenderScene scene{.meshes = items, .rect_lights = rects,
                            .sun = {.direction = {-0.4f, -0.8f, -0.3f}, .intensity = 2.0f},
                            .environment = {.sky = {0.25f, 0.35f, 0.5f}, .ground = {0.1f, 0.1f, 0.1f}}};

    const auto pt_px = rig.draw(scene, 24);
    CHECK(rig.renderer->stats().path_tracer_samples == 24u);
    const float pt_luma = PbrRig::mean_luma(pt_px);
    CHECK(pt_luma > 30.0f);

    // Moving the camera must automatically reset path tracer accumulation to 1 sample.
    rig.look_from({0.5f, 1.5f, 4.5f}, {0, 1.0f, 0});
    (void)rig.draw(scene, 1);
    CHECK(rig.renderer->stats().path_tracer_samples == 1u);

    // Scene changes must restart accumulation even with the same camera and light count.
    auto moved_items = items;
    auto moved_lights = rects;
    RenderScene changed = scene;
    changed.meshes = moved_items;
    changed.rect_lights = moved_lights;
    (void)rig.draw(changed, 2);
    REQUIRE(rig.renderer->stats().path_tracer_samples == 3u);
    moved_items[1].transform = translate({1, 0.6f, 0});
    (void)rig.draw(changed, 2);
    CHECK(rig.renderer->stats().path_tracer_samples == 2u);
    moved_lights[0].position.x += 0.5f;
    (void)rig.draw(changed, 1);
    CHECK(rig.renderer->stats().path_tracer_samples == 1u);
    REQUIRE(rig.renderer->update_material(box_m, {.base_color = {0.2f, 0.5f, 0.8f, 1}}));
    (void)rig.draw(changed, 1);
    CHECK(rig.renderer->stats().path_tracer_samples == 1u);
    REQUIRE(rig.renderer->update_material(box_m, {.base_color = {0.82f, 0.22f, 0.18f, 1}, .roughness = 0.55f}));

    // Compare Hybrid RT mean luminance against converged Path Tracer Reference at the original pose.
    rig.look_from({0, 1.5f, 4.5f}, {0, 1.0f, 0});
    rig.renderer->set_settings(rt_settings(false));
    const auto rt_px = rig.draw(scene, 16);
    CHECK(rig.renderer->stats().path_tracer_samples == 0u);
    const float rt_luma = PbrRig::mean_luma(rt_px);
    const float rel_diff = std::abs(rt_luma - pt_luma) / std::max(pt_luma, 1.0f);
    CHECK_MESSAGE(rel_diff < 0.30f, "rt_luma=", rt_luma, " pt_luma=", pt_luma, " rel_diff=", rel_diff);
}

TEST_CASE("M13 PBR: custom .ashader shading_model compiles and renders across raster, RT, and PT") {
    const char* src = R"(shader_type surface;
shading_model clearcoat;
uniform float coat_strength = 1.0;
void surface() {
    ALBEDO = vec3(0.08, 0.12, 0.25);
    ROUGHNESS = 0.8;
    CLEARCOAT = coat_strength;
    CLEARCOAT_ROUGHNESS = 0.04;
    WETNESS = 0.2;
}
)";
    const ShaderParse parsed = parse_ashader(src);
    REQUIRE(parsed.errors.empty());
    CHECK(parsed.source.shading_model == ShadingModel::Clearcoat);
    const SurfaceShaderBuild build = compile_surface_shader(parsed.source, "m13_coat.ashader");
    REQUIRE_MESSAGE(build.ok(), (build.errors.empty() ? std::string() : build.errors[0].message));

    const RtSurface rt_list[] = {{0, build.rt_chunk, build.alpha_test}};
    const RtVariants vars = compile_rt_variants(rt_list);
    REQUIRE_MESSAGE(vars.ok(), (vars.errors.empty() ? std::string() : vars.errors[0].message));

    PbrRig rig(rt_settings(false));
    if (!rig.device) return;
    const bool has_rt = rig.renderer->ray_tracing();
    const ShaderHandle sh = rig.renderer->create_shader(build, "m13_coat");
    REQUIRE(sh.valid());
    const MaterialHandle mat = rig.renderer->create_material({.shader = sh});
    const MeshHandle sphere = rig.renderer->create_mesh(make_sphere(24, 12));
    const std::array items{MeshInstance{sphere, mat, scale({2, 2, 2})}};
    const RenderScene scene{.meshes = items, .sun = {.direction = {0, 0, -1}, .intensity = 10.0f, .shadows = false}};
    for (int mode = 0; mode < (has_rt ? 3 : 1); ++mode) {
        rig.renderer->set_settings(mode == 0 ? RenderSettings{.ray_tracing = false} : rt_settings(mode == 2));
        const auto px = rig.draw(scene, mode == 2 ? 8 : (mode == 1 ? 4 : 1));
        CHECK(PbrRig::luma(rig.at(px, {0, 0, 0.99f})) > 80.0f);
    }
    rig.renderer->destroy(sh);
}

TEST_CASE("path tracing only resets when time changes for a time-dependent surface") {
    PbrRig rig(rt_settings(true));
    if (!rig.device || !rig.renderer->ray_tracing()) return;
    const auto parsed = parse_ashader(
        "shader_type surface;\nrender_mode unshaded;\nvoid surface() { ALBEDO = vec3(TIME, 0.0, 0.0); }");
    REQUIRE(parsed.errors.empty());
    const ShaderHandle shader = rig.renderer->create_shader(compile_surface_shader(parsed.source, "animated"), "animated");
    REQUIRE(shader.valid());
    const MeshHandle cube = rig.renderer->create_mesh(make_cube());
    MeshInstance item{.mesh = cube, .material = rig.renderer->default_material()};
    RenderScene scene{.meshes = std::span(&item, 1)};
    (void)rig.draw(scene, 1);
    scene.time = 0.25f;
    (void)rig.draw(scene, 1);
    CHECK(rig.renderer->stats().path_tracer_samples == 2u);

    item.material = rig.renderer->create_material({.shader = shader});
    (void)rig.draw(scene, 2);
    CHECK(rig.renderer->stats().path_tracer_samples == 2u);
    scene.time = 0.75f;
    (void)rig.draw(scene, 1);
    CHECK(rig.renderer->stats().path_tracer_samples == 1u);
}

TEST_CASE("M13 PBR: path tracer renders custom sky and forward blended glass/hologram") {
    PbrRig rig(rt_settings(true));
    if (!rig.device || !rig.renderer->ray_tracing()) return;
    const ShaderParse sky_p = parse_ashader("shader_type sky;\nvoid sky() { COLOR = vec3(0.05, 0.25, 2.5); }\n");
    const ShaderParse blend_p = parse_ashader(
        "shader_type surface;\nrender_mode blend_mix, unshaded;\n"
        "void surface() { ALBEDO = vec3(2.0, 0.1, 0.1); ALPHA = 0.85; }\n");
    REQUIRE(sky_p.errors.empty());
    REQUIRE(blend_p.errors.empty());
    const ShaderHandle sky_sh = rig.renderer->create_shader(compile_screen_shader(sky_p.source, "pt_sky"), "pt_sky");
    const ShaderHandle blend_sh = rig.renderer->create_shader(compile_surface_shader(blend_p.source, "pt_blend"), "pt_blend");
    REQUIRE(sky_sh.valid());
    REQUIRE(blend_sh.valid());
    const MaterialHandle sky_mat = rig.renderer->create_material({.shader = sky_sh});
    const MaterialHandle blend_mat = rig.renderer->create_material({.shader = blend_sh});
    const MeshHandle cube = rig.renderer->create_mesh(make_cube());
    const std::array items{MeshInstance{cube, blend_mat, scale({1.2f, 1.2f, 1.2f})}};
    const RenderScene scene{.meshes = items, .sun = {.intensity = 0.0f},
                            .environment = {.sky = {0, 0, 0}, .ground = {0, 0, 0}}, .sky = sky_mat};
    const auto px = rig.draw(scene, 4);
    const auto bg = PbrRig::rgb(px[4 * PbrRig::kSize + 4]);
    const auto fg = PbrRig::rgb(rig.at(px, {0, 0, 0}));
    CHECK_MESSAGE(bg[2] > bg[0] + 60, "pt sky=(", bg[0], ",", bg[1], ",", bg[2], ")");
    // A 2.5-linear blue background contributes 0.375 before tonemapping at 85% opacity.
    // Blending the already tonemapped colors would lose most of that blue contribution.
    CHECK(fg[2] > 170);
    CHECK_MESSAGE(fg[0] > fg[2] + 20, "pt blend=(", fg[0], ",", fg[1], ",", fg[2], ")");
    rig.renderer->destroy(sky_sh);
    rig.renderer->destroy(blend_sh);
}
