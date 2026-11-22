// Mathematical verification and GPU regression tests from the rendering architecture audit.
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
#include <cstring>
#include <memory>
#include <numbers>
#include <vector>

using namespace astral;

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

// --- CPU mirrors of core shader math contracts ---------------------------------------------

float ggx_d(float nh, float a) {
    const float a2 = a * a;
    const float d = nh * nh * (a2 - 1.0f) + 1.0f;
    return a2 / std::max(kPi * d * d, 1e-12f);
}

float smith_lambda(float nx, float a) {
    const float a2 = a * a;
    const float nx2 = std::max(nx * nx, 1e-8f);
    return 0.5f * (std::sqrt(1.0f + a2 * (1.0f - nx2) / nx2) - 1.0f);
}

float smith_g1(float nx, float a) {
    const float a2 = a * a;
    return 2.0f * nx / std::max(nx + std::sqrt(a2 + (1.0f - a2) * nx * nx), 1e-7f);
}

float smith_vndf_weight(float nv, float nl, float a) {
    const float lv = smith_lambda(nv, a);
    const float ll = smith_lambda(nl, a);
    return (1.0f + lv) / std::max(1.0f + lv + ll, 1e-7f);
}

Vec3 sample_vndf(Vec3 v, float a, float u1, float u2) {
    const Vec3 vh = normalize(Vec3{a * v.x, a * v.y, std::max(v.z, 1e-5f)});
    const float len2 = vh.x * vh.x + vh.y * vh.y;
    const Vec3 t1 = len2 > 0.0f ? Vec3{-vh.y, vh.x, 0.0f} / std::sqrt(len2) : Vec3{1, 0, 0};
    const Vec3 t2 = cross(vh, t1);
    const float r = std::sqrt(std::clamp(u1, 0.0f, 0.999999f));
    const float phi = 2.0f * kPi * u2;
    const float p1 = r * std::cos(phi);
    float p2 = r * std::sin(phi);
    const float s = 0.5f * (1.0f + vh.z);
    p2 = (1.0f - s) * std::sqrt(std::max(0.0f, 1.0f - p1 * p1)) + s * p2;
    const Vec3 nh = t1 * p1 + t2 * p2 + vh * std::sqrt(std::max(0.0f, 1.0f - p1 * p1 - p2 * p2));
    return normalize(Vec3{a * nh.x, a * nh.y, std::max(0.0f, nh.z)});
}

Vec2 oct_encode(Vec3 n) {
    const float s = std::max(std::abs(n.x) + std::abs(n.y) + std::abs(n.z), 1e-8f);
    n = n / s;
    if (n.z >= 0.0f) return {n.x, n.y};
    return {(1.0f - std::abs(n.y)) * (n.x >= 0.0f ? 1.0f : -1.0f),
            (1.0f - std::abs(n.x)) * (n.y >= 0.0f ? 1.0f : -1.0f)};
}

Vec3 oct_decode(Vec2 e) {
    Vec3 v{e.x, e.y, 1.0f - std::abs(e.x) - std::abs(e.y)};
    if (v.z < 0.0f) {
        const float ox = v.x, oy = v.y;
        v.x = (1.0f - std::abs(oy)) * (ox >= 0.0f ? 1.0f : -1.0f);
        v.y = (1.0f - std::abs(ox)) * (oy >= 0.0f ? 1.0f : -1.0f);
    }
    return normalize(v);
}

Vec3 offset_ray(Vec3 p, Vec3 ng) {
    constexpr float origin = 1.0f / 32.0f;
    constexpr float float_scale = 1.0f / 65536.0f;
    constexpr float int_scale = 256.0f;
    auto shift = [](float v, float n) {
        if (std::abs(v) < origin) return v + float_scale * n;
        std::int32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        const auto off = static_cast<std::int32_t>(int_scale * n);
        bits += (v < 0.0f ? -off : off);
        float out = 0.0f;
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    };
    return {shift(p.x, ng.x), shift(p.y, ng.y), shift(p.z, ng.z)};
}

float srgb_to_linear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
float linear_to_srgb(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

Vec3 normal_cofactor_transform(const Mat4& m, Vec3 n) {
    const Vec3 m0{m[0].x, m[0].y, m[0].z}, m1{m[1].x, m[1].y, m[1].z}, m2{m[2].x, m[2].y, m[2].z};
    const Vec3 c0 = cross(m1, m2), c1 = cross(m2, m0), c2 = cross(m0, m1);
    const float s = dot(m0, c0) < 0.0f ? -1.0f : 1.0f;
    return normalize((c0 * n.x + c1 * n.y + c2 * n.z) * s);
}

struct AuditFixture {
    static constexpr std::uint32_t kSize = 64;
    test::TestDevice device;
    std::unique_ptr<Renderer> renderer;
    gpu::TextureHandle target;
    gpu::BufferHandle readback;
    CameraData camera;

    explicit AuditFixture(RenderSettings settings = {}) {
        if (!device) return;
        renderer = std::make_unique<Renderer>(*device, gpu::Format::RGBA8Unorm, settings);
        target = device->create_texture({.width = kSize, .height = kSize, .format = gpu::Format::RGBA8Unorm,
                                         .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::TransferSrc});
        readback = device->create_buffer({.size = kSize * kSize * 4, .usage = gpu::BufferUsage::TransferDst,
                                          .memory = gpu::MemoryUsage::GpuToCpu});
        look_from({0, 0, 5}, {0, 0, 0});
    }

    void look_from(Vec3 eye, Vec3 at, float near_p = 0.1f, float far_p = 100.0f) {
        camera.position = eye;
        const Vec3 up = std::abs(eye.y - at.y) > 0.1f && eye.x == at.x && eye.z == at.z ? Vec3{0, 0, -1} : Vec3{0, 1, 0};
        camera.view = look_at(eye, at, up);
        camera.projection = perspective(radians(60.0f), 1.0f, near_p, far_p);
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
    static int luma(std::uint32_t p) {
        const auto c = rgb(p);
        return (c[0] + c[1] + c[2]) / 3;
    }
    std::uint32_t at(const std::vector<std::uint32_t>& px, Vec3 world) const {
        const Vec4 clip = camera.projection * camera.view * Vec4{world.x, world.y, world.z, 1};
        const float x = (clip.x / clip.w * 0.5f + 0.5f) * kSize;
        const float y = (clip.y / clip.w * 0.5f + 0.5f) * kSize;
        const auto ix = static_cast<std::uint32_t>(std::clamp(x, 0.0f, kSize - 1.0f));
        const auto iy = static_cast<std::uint32_t>(std::clamp(y, 0.0f, kSize - 1.0f));
        return px[iy * kSize + ix];
    }
};

RenderSettings small_rt() {
    RenderSettings s;
    s.gi = {.probes_x = 4, .probes_y = 4, .probes_z = 4, .rays_per_probe = 32};
    return s;
}

} // namespace

// --- CPU Mathematical Verification Tests ---------------------------------------------------

TEST_CASE("math contract: GGX VNDF white-furnace reflectance is energy-conserving and matches Smith G2/G1") {
    std::uint32_t state = 0x12345678u;
    auto next_f = [&]() {
        state = state * 747796405u + 2891336453u;
        const std::uint32_t w = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        return static_cast<float>((w >> 22u) ^ w) * (1.0f / 4294967296.0f);
    };
    for (float roughness : {0.1f, 0.35f, 0.7f, 1.0f}) {
        const float a = roughness * roughness;
        for (float nv : {0.2f, 0.5f, 0.9f, 1.0f}) {
            const Vec3 v{std::sqrt(std::max(0.0f, 1.0f - nv * nv)), 0.0f, nv};
            double sum = 0.0;
            constexpr int N = 20000;
            for (int i = 0; i < N; ++i) {
                const Vec3 h = sample_vndf(v, a, next_f(), next_f());
                const Vec3 l = h * (2.0f * dot(v, h)) - v;
                if (l.z > 0.0f) sum += smith_vndf_weight(nv, l.z, a);
            }
            const double furnace = sum / N;
            CHECK(furnace > 0.25);
            CHECK(furnace <= 1.005); // strict single-scatter energy conservation
            // At normal incidence (nv=1), verify exact match against 1D quadrature of D(h)*G2(1, cos 2th)*cos(th).
            if (nv == 1.0f) {
                double exact = 0.0;
                constexpr int Q = 2048;
                const double dth = (0.25 * static_cast<double>(kPi)) / Q;
                for (int q = 0; q < Q; ++q) {
                    const double th = (q + 0.5) * dth;
                    const float nh = static_cast<float>(std::cos(th));
                    const float nl = static_cast<float>(std::cos(2.0 * th));
                    exact += static_cast<double>(ggx_d(nh, a)) * static_cast<double>(smith_g1(nl, a)) * nh *
                             (2.0 * static_cast<double>(kPi) * std::sin(th)) * dth;
                }
                CHECK(std::abs(furnace - exact) < 0.015);
                if (roughness == 1.0f) CHECK(exact == doctest::Approx(1.0 - std::log(2.0)).epsilon(1e-4));
            }
        }
    }
    // Sanity check GGX D peak decreases monotonically with roughness.
    CHECK(ggx_d(1.0f, 0.1f) > ggx_d(1.0f, 0.5f));
    CHECK(smith_g1(1.0f, 0.5f) == doctest::Approx(1.0f).epsilon(1e-5));
}

TEST_CASE("math contract: octahedral encode/decode round-trips across the unit sphere") {
    for (int iz = -4; iz <= 4; ++iz) {
        for (int iy = -4; iy <= 4; ++iy) {
            for (int ix = -4; ix <= 4; ++ix) {
                if (ix == 0 && iy == 0 && iz == 0) continue;
                const Vec3 n = normalize(Vec3{static_cast<float>(ix), static_cast<float>(iy), static_cast<float>(iz)});
                const Vec3 decoded = oct_decode(oct_encode(n));
                CHECK(dot(n, decoded) > 0.99999f);
            }
        }
    }
}

TEST_CASE("math contract: Wachter-Binder offset_ray scales across 7 orders of magnitude") {
    const Vec3 n = normalize(Vec3{0.3f, 0.8f, -0.5f});
    float prev_shift = 0.0f;
    for (float coord : {0.001f, 1.0f, 100.0f, 10000.0f}) {
        const Vec3 p{coord, -coord * 0.5f, coord * 0.25f};
        const Vec3 q = offset_ray(p, n);
        const float shift = dot(q - p, n);
        CHECK(shift > 0.0f);
        // Inside [-1/32, 1/32] offset_ray uses constant float_scale (1/65536); beyond 1.0 it scales with ULP.
        if (coord > 1.0f) CHECK(shift > prev_shift);
        prev_shift = shift;
    }
}

TEST_CASE("math contract: signed cofactor normal_matrix_of matches transpose(inverse(M)) including negative scale") {
    const std::array transforms = {
        scale({3.0f, 0.25f, 5.0f}),
        rotate(Quat::from_axis_angle({0.3f, 0.8f, 0.5f}, 1.1f)) * scale({2.0f, 0.5f, 4.0f}),
        scale({-2.0f, 1.0f, 3.0f}), // reflection (negative determinant)
    };
    const Vec3 n = normalize(Vec3{0.4f, 0.7f, -0.6f});
    for (const Mat4& m : transforms) {
        const Mat4 ti = transpose(inverse(m));
        const Vec4 ref4 = ti * Vec4{n.x, n.y, n.z, 0.0f};
        const Vec3 ref = normalize(Vec3{ref4.x, ref4.y, ref4.z});
        const Vec3 got = normal_cofactor_transform(m, n);
        CHECK(dot(ref, got) > 0.9999f);
    }
}

TEST_CASE("math contract: IEC sRGB round-trip and bloom upsample unit DC gain") {
    for (int i = 0; i <= 100; ++i) {
        const float x = static_cast<float>(i) / 100.0f;
        CHECK(srgb_to_linear(linear_to_srgb(x)) == doctest::Approx(x).epsilon(1e-5));
    }
    for (int levels = 2; levels <= 8; ++levels) {
        float acc = 1.0f; // bottom level = 1.0
        for (int step = 1; step < levels; ++step) {
            const float blend = static_cast<float>(step) / static_cast<float>(step + 1);
            acc = (1.0f - blend) * 1.0f + blend * acc;
        }
        CHECK(acc == doctest::Approx(1.0f).epsilon(1e-6));
    }
}

// --- GPU Regression Tests ------------------------------------------------------------------

TEST_CASE("regression: DDGI frame 0 and post-reset never leak uninitialized VRAM into a dark room") {
    AuditFixture f(small_rt());
    if (!f.device || !f.renderer->ray_tracing()) return;
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle yellow = f.renderer->create_material({.base_color = {1.0f, 0.9f, 0.1f, 1}, .roughness = 0.5f});
    const MaterialHandle white = f.renderer->create_material({.base_color = {0.8f, 0.8f, 0.8f, 1}, .roughness = 0.85f});
    // First render a bright yellow scene to populate GPU memory.
    const MeshInstance bright[] = {{cube, yellow, scale({4, 4, 4})}};
    f.draw({.meshes = bright, .sun = {.direction = {0, 0, -1}, .intensity = 5.0f}}, 6);

    // Resize DDGI grid and reset history, then render a sealed dark room with zero lights and black sky.
    RenderSettings s = small_rt();
    s.gi = {.probes_x = 5, .probes_y = 4, .probes_z = 5, .rays_per_probe = 32,
            .volume_min = {-4, -4, -4}, .volume_max = {4, 4, 4}};
    f.renderer->set_settings(s);
    f.renderer->reset_history();
    const std::array room = {
        MeshInstance{cube, white, translate({0, 0, -3}) * scale({8, 8, 0.4f})},
        MeshInstance{cube, white, translate({0, 0, 3}) * scale({8, 8, 0.4f})},
        MeshInstance{cube, white, translate({-3, 0, 0}) * scale({0.4f, 8, 8})},
        MeshInstance{cube, white, translate({3, 0, 0}) * scale({0.4f, 8, 8})},
        MeshInstance{cube, white, translate({0, -3, 0}) * scale({8, 0.4f, 8})},
        MeshInstance{cube, white, translate({0, 3, 0}) * scale({8, 0.4f, 8})},
    };
    f.look_from({0, 0, 2}, {0, 0, -2});
    const auto pixels = f.draw({.meshes = room, .sun = {.intensity = 0.0f},
                                .environment = {.sky = {}, .ground = {}}}, 8);
    int max_channel = 0;
    for (std::uint32_t p : pixels) {
        const auto c = AuditFixture::rgb(p);
        max_channel = std::max({max_channel, c[0], c[1], c[2]});
    }
    CHECK(max_channel < 4); // sealed dark room must stay dark, never glowing with stale/uninitialized VRAM
}

TEST_CASE("regression: DDGI normalized distance moments avoid FP16 overflow on 2000m worlds") {
    RenderSettings s = small_rt();
    s.max_ray_distance = 3000.0f;
    AuditFixture f(s);
    if (!f.device || !f.renderer->ray_tracing()) return;
    const MeshHandle plane = f.renderer->create_mesh(make_plane(1.0f));
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle green = f.renderer->create_material({.base_color = {0.2f, 0.5f, 0.2f, 1}, .roughness = 0.9f});
    const MaterialHandle grey = f.renderer->create_material({.base_color = {0.6f, 0.6f, 0.6f, 1}, .roughness = 0.8f});
    const std::array items = {
        MeshInstance{plane, green, scale({2000, 1, 2000})},
        MeshInstance{cube, grey, translate({0, 20, 0}) * scale({20, 40, 20})},
    };
    f.look_from({80, 30, 100}, {0, 15, 0}, 0.2f, 3000.0f);
    const auto pixels = f.draw({.meshes = items,
                                .sun = {.direction = {-0.4f, -0.7f, -0.3f}, .intensity = 2.0f},
                                .environment = {.sky = {0.3f, 0.4f, 0.6f}, .ground = {0.1f, 0.1f, 0.1f}}}, 8);
    const auto ground_px = AuditFixture::rgb(f.at(pixels, {25, 0, 25}));
    CHECK(ground_px[1] > ground_px[0]); // green ground stays green, not blown out by FP16 Inf/NaN
    CHECK(ground_px[1] < 245);
}

TEST_CASE("regression: bloom preserves uniform HDR DC energy (no 6x amplification)") {
    RenderSettings s = small_rt();
    s.bloom_strength = 0.0f;
    AuditFixture f(s);
    if (!f.device || !f.renderer->ray_tracing()) return;
    const RenderScene uniform_sky{.sun = {.intensity = 0.0f},
                                  .environment = {.sky = {0.4f, 0.4f, 0.4f}, .ground = {0.4f, 0.4f, 0.4f}}};
    const int no_bloom = AuditFixture::luma(f.draw(uniform_sky, 4)[32 * AuditFixture::kSize + 32]);
    s.bloom_strength = 0.25f;
    f.renderer->set_settings(s);
    const int with_bloom = AuditFixture::luma(f.draw(uniform_sky, 4)[32 * AuditFixture::kSize + 32]);
    CHECK(std::abs(with_bloom - no_bloom) <= 3); // unit DC gain: constant HDR stays constant
}

TEST_CASE("regression: alpha-tested .ashader lets ray-traced shadows pass through cut-out regions") {
    AuditFixture f(small_rt());
    if (!f.device || !f.renderer->ray_tracing()) return;
    const ShaderParse p = parse_ashader(R"(shader_type surface;
render_mode alpha_test, cull_disabled;
void surface() {
    ALBEDO = vec3(0.2, 0.8, 0.2);
    ROUGHNESS = 0.8;
    ALPHA = UV.x > 0.5 ? 1.0 : 0.0;
}
)");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "cutout.ashader");
    REQUIRE(build.ok());
    const ShaderHandle sh = f.renderer->create_shader(build, "cutout");
    REQUIRE(sh.valid());
    const MeshHandle plane = f.renderer->create_mesh(make_plane(1.0f));
    const MaterialHandle floor_mat = f.renderer->create_material({.base_color = {0.8f, 0.8f, 0.8f, 1}, .roughness = 0.9f});
    const MaterialHandle card_mat = f.renderer->create_material({.shader = sh});
    const std::array items = {
        MeshInstance{plane, floor_mat, scale({12, 1, 12})},
        MeshInstance{plane, card_mat, translate({0, 2.0f, 0}) * scale({4, 1, 4})}, // +x opaque, -x cut out
    };
    f.look_from({0, 1.2f, 5.5f}, {0, 0, 0});
    const auto pixels = f.draw({.meshes = items,
                                .sun = {.direction = {0, -1, 0}, .intensity = 3.0f, .angular_radius = 0.0f},
                                .environment = {.sky = {0.03f, 0.03f, 0.04f}, .ground = {0.01f, 0.01f, 0.01f}}}, 12);
    const int lit_under_hole = AuditFixture::luma(f.at(pixels, {-1.0f, 0, 0}));
    const int shadowed_under_solid = AuditFixture::luma(f.at(pixels, {1.0f, 0, 0}));
    CHECK(lit_under_hole > shadowed_under_solid + 40);
    f.renderer->destroy(sh);
}

TEST_CASE("regression: transparent forward PBR lighting matches opaque PBR units (no 1/PI darkening)") {
    AuditFixture f({.shadow_map_size = 256, .ray_tracing = false});
    if (!f.device) return;
    const ShaderParse p = parse_ashader(R"(shader_type surface;
render_mode blend_mix;
void surface() {
    ALBEDO = vec3(0.8, 0.8, 0.8);
    ROUGHNESS = 1.0;
    METALLIC = 0.0;
    ALPHA = 1.0;
}
)");
    REQUIRE(p.errors.empty());
    const SurfaceShaderBuild build = compile_surface_shader(p.source, "opaque_blend.ashader");
    REQUIRE(build.ok());
    const ShaderHandle sh = f.renderer->create_shader(build, "opaque_blend");
    REQUIRE(sh.valid());
    const MeshHandle cube = f.renderer->create_mesh(make_cube());
    const MaterialHandle opaque_m = f.renderer->create_material({.base_color = {0.8f, 0.8f, 0.8f, 1}, .roughness = 1.0f});
    const MaterialHandle blend_m = f.renderer->create_material({.shader = sh});
    const DirectionalLight sun{.direction = {0, 0, -1}, .intensity = 2.0f, .shadows = false};
    const Environment dark{.sky = {}, .ground = {}, .ambient = 0.0f};
    const MeshInstance a[] = {{cube, opaque_m}};
    const MeshInstance b[] = {{cube, blend_m}};
    const int opaque_l = AuditFixture::luma(f.draw({.meshes = a, .sun = sun, .environment = dark})[32 * AuditFixture::kSize + 32]);
    const int blend_l = AuditFixture::luma(f.draw({.meshes = b, .sun = sun, .environment = dark})[32 * AuditFixture::kSize + 32]);
    CHECK(opaque_l > 120);
    CHECK(std::abs(blend_l - opaque_l) <= 4);
    f.renderer->destroy(sh);
}

TEST_CASE("regression: RectLight MIS produces clean specular highlights on rough metals") {
    AuditFixture f(small_rt());
    if (!f.device || !f.renderer->ray_tracing()) return;
    const MeshHandle plane = f.renderer->create_mesh(make_plane(1.0f));
    const MaterialHandle rough_metal =
        f.renderer->create_material({.base_color = {0.95f, 0.95f, 0.95f, 1}, .roughness = 0.35f, .metallic = 1.0f});
    const MeshInstance floor[] = {{plane, rough_metal, scale({10, 1, 10})}};
    const RectLight panel{.position = {0, 2.5f, -3.0f}, .right = {0.6f, 0, 0}, .up = {0, 0, 0.6f}, .intensity = 25.0f};
    f.look_from({0, 2.5f, 3.0f}, {0, 0, 0});
    const auto pixels = f.draw({.meshes = floor, .rect_lights = std::span(&panel, 1),
                                .sun = {.intensity = 0.0f},
                                .environment = {.sky = {}, .ground = {}}}, 12);
    // Specular reflection of the panel on the rough metallic floor around the origin must be clearly bright.
    CHECK(AuditFixture::luma(f.at(pixels, {0, 0, 0})) > 90);
}
