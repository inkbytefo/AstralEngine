// GPU rendering validation & performance benchmark harness.
// Usage: astral-render-bench [--out <dir>] [--width W] [--height H] [--frames N]
// Renders controlled scenes on the active GPU, prints per-pass GPU timings and
// quantitative stability/energy metrics, and optionally writes PPM captures.

#include <astral/core/math.hpp>
#include <astral/gpu/device.hpp>
#include <astral/render/mesh.hpp>
#include <astral/render/renderer.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#if defined(_MSC_VER)
#include <share.h>
#endif

using namespace astral;

namespace {

bool write_ppm(const std::string& path, const std::vector<std::uint8_t>& rgba, std::uint32_t w, std::uint32_t h) {
#if defined(_MSC_VER)
    std::FILE* f = _fsopen(path.c_str(), "wb", _SH_DENYNO);
#else
    std::FILE* f = std::fopen(path.c_str(), "wb");
#endif
    if (!f) return false;
    std::fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (std::size_t p = 0; p < std::size_t{w} * h; ++p) std::fwrite(&rgba[p * 4], 1, 3, f);
    std::fclose(f);
    return true;
}

Mat4 box_tf(Vec3 center, Vec3 sz, float yaw_deg = 0.0f) {
    return translate(center) * rotate(Quat::from_axis_angle({0, 1, 0}, radians(yaw_deg))) * scale(sz);
}

struct BenchContext {
    std::unique_ptr<gpu::Device> device;
    std::unique_ptr<Renderer> renderer;
    gpu::TextureHandle target;
    gpu::BufferHandle readback;
    MeshHandle cube, sphere, plane;
    std::uint32_t width = 1280, height = 720;
    std::string out_dir;

    bool init(std::uint32_t w, std::uint32_t h, const std::string& out) {
        width = w;
        height = h;
        out_dir = out;
        device = gpu::Device::create({.app_name = "astral-render-bench", .validation = false});
        if (!device) return false;
        renderer = std::make_unique<Renderer>(*device, gpu::Format::RGBA8Unorm, RenderSettings{});
        target = device->create_texture({.width = width, .height = height, .format = gpu::Format::RGBA8Unorm,
                                         .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::TransferSrc});
        readback = device->create_buffer({.size = std::uint64_t{width} * height * 4,
                                          .usage = gpu::BufferUsage::TransferDst, .memory = gpu::MemoryUsage::GpuToCpu});
        cube = renderer->create_mesh(make_cube());
        sphere = renderer->create_mesh(make_sphere(64, 48));
        plane = renderer->create_mesh(make_plane(1.0f));
        return true;
    }

    CameraData make_camera(Vec3 eye, Vec3 at, float fov_deg = 45.0f, float near_p = 0.05f, float far_p = 200.0f) const {
        CameraData c;
        c.position = eye;
        c.view = look_at(eye, at, {0, 1, 0});
        c.projection = perspective(radians(fov_deg), static_cast<float>(width) / static_cast<float>(height), near_p, far_p);
        return c;
    }

    std::vector<std::uint8_t> capture_target() {
        auto cmd = device->begin_commands();
        cmd.transition(target, gpu::ResourceState::TransferSrc);
        cmd.copy_texture_to_buffer(target, readback);
        device->wait(device->submit(cmd));
        std::vector<std::uint8_t> rgba(std::size_t{width} * height * 4);
        device->read_buffer(readback, rgba.data(), rgba.size());
        return rgba;
    }

    struct PerfStats {
        double gpu_frame_ms = 0.0;
        double wall_frame_ms = 0.0;
        std::map<std::string, double> pass_ms;
    };

    PerfStats run_scene(const char* name, const RenderSettings& settings, const CameraData& cam,
                        const RenderScene& scene, int warmup_frames, int measure_frames) {
        renderer->set_settings(settings);
        renderer->reset_history();
        for (int i = 0; i < warmup_frames; ++i) {
            auto cmd = device->begin_commands();
            renderer->render(cmd, target, cam, scene);
            device->wait(device->submit(cmd));
        }

        double gpu_sum = 0.0;
        int gpu_samples = 0;
        std::map<std::string, double> pass_sums;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < measure_frames; ++i) {
            auto cmd = device->begin_commands();
            renderer->render(cmd, target, cam, scene);
            device->wait(device->submit(cmd));
            if (renderer->gpu_frame_ms() > 0.0f) {
                gpu_sum += renderer->gpu_frame_ms();
                ++gpu_samples;
                for (const PassTiming& pt : renderer->gpu_timings()) pass_sums[pt.name] += pt.ms;
            }
        }
        const double wall_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / measure_frames;

        PerfStats ps;
        ps.wall_frame_ms = wall_ms;
        if (gpu_samples > 0) {
            ps.gpu_frame_ms = gpu_sum / gpu_samples;
            for (const auto& [k, v] : pass_sums) ps.pass_ms[k] = v / gpu_samples;
        }
        const RenderStats& st = renderer->stats();
        std::printf("\n[scene: %s] %ux%u rt=%d probes=%u lights=%u rects=%u sdfs=%u passes=%u\n",
                    name, width, height, st.ray_traced ? 1 : 0, st.gi_probes, st.point_lights, st.rect_lights,
                    st.sdf_shapes, st.passes);
        std::printf("  gpu_frame_ms=%.3f ms (%.1f FPS) | wall_ms=%.3f ms\n",
                    ps.gpu_frame_ms, ps.gpu_frame_ms > 0.0 ? 1000.0 / ps.gpu_frame_ms : 0.0, ps.wall_frame_ms);
        for (const auto& [k, v] : ps.pass_ms) {
            std::printf("    %-24s %.3f ms\n", k.c_str(), v);
        }
        if (!out_dir.empty()) {
            const auto rgba = capture_target();
            write_ppm(out_dir + "/" + name + ".ppm", rgba, width, height);
        }
        return ps;
    }
};

void scene_cornell_area(BenchContext& b, int warmup, int measure) {
    auto mat = [&](Vec4 col, float r, float m = 0.0f) {
        return b.renderer->create_material({.base_color = col, .roughness = r, .metallic = m});
    };
    const float W = 6.0f, H = 5.0f, D = 9.0f, t = 0.2f, zc = 1.0f;
    const Vec4 white{0.78f, 0.78f, 0.76f, 1}, red{0.82f, 0.06f, 0.05f, 1}, green{0.08f, 0.62f, 0.12f, 1},
        blue{0.06f, 0.18f, 0.85f, 1};
    std::vector<MeshInstance> meshes = {
        {b.cube, mat(white, 0.35f), box_tf({0, -t / 2, zc}, {W + 2 * t, t, D})},
        {b.cube, mat(white, 0.9f), box_tf({0, H + t / 2, zc}, {W + 2 * t, t, D})},
        {b.cube, mat(white, 0.9f), box_tf({0, H / 2, zc - D / 2 - t / 2}, {W + 2 * t, H, t})},
        {b.cube, mat(red, 0.85f), box_tf({-W / 2 - t / 2, H / 2, zc}, {t, H, D})},
        {b.cube, mat(green, 0.85f), box_tf({W / 2 + t / 2, H / 2, zc}, {t, H, D})},
        {b.cube, mat(white, 0.6f), box_tf({0, 0.25f, -0.4f}, {1.6f, 0.5f, 1.6f})},
        {b.sphere, mat({0.97f, 0.97f, 0.97f, 1}, 0.02f, 1.0f), translate({0, 1.45f, -0.4f}) * scale({1.9f, 1.9f, 1.9f})},
        {b.cube, mat(blue, 0.18f), box_tf({-1.75f, 1.6f, -2.0f}, {1.3f, 3.2f, 1.3f}, 22)},
        {b.cube, mat(white, 0.7f), box_tf({1.8f, 0.6f, 0.9f}, {1.2f, 1.2f, 1.2f}, -18)},
        {b.cube, mat({0.95f, 0.64f, 0.54f, 1}, 0.32f, 1.0f), box_tf({1.75f, 1.55f, 0.85f}, {0.6f, 0.6f, 0.6f}, 30)},
    };
    std::vector<SdfShape> sdfs = {
        {.primitive = SdfPrimitive::Torus, .operation = SdfOperation::Union, .position = {-0.9f, 0.32f, 1.6f},
         .rotation = Quat::from_axis_angle({1, 0, 0.3f}, radians(70)), .size = {0.45f, 0.14f, 0, 0},
         .color = {1.0f, 0.78f, 0.34f, 1}, .roughness = 0.22f, .metallic = 1.0f},
        {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::Union, .position = {0.65f, 0.28f, 2.1f},
         .size = {0.28f, 0, 0, 0}, .color = {0.85f, 0.1f, 0.08f, 1}, .roughness = 0.45f},
        {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::SmoothUnion, .position = {1.05f, 0.22f, 2.25f},
         .size = {0.22f, 0, 0, 0}, .blend = 0.18f, .color = {0.95f, 0.85f, 0.2f, 1}, .roughness = 0.45f},
    };
    RectLight panel{.position = {0, H - 0.01f, -0.3f}, .right = {0.85f, 0, 0}, .up = {0, 0, 0.85f},
                    .color = {1.0f, 0.93f, 0.84f}, .intensity = 18.0f};
    RenderSettings s;
    s.gi = {.probes_x = 8, .probes_y = 7, .probes_z = 10, .rays_per_probe = 128,
            .volume_min = {-3.0f, 0.0f, -3.5f}, .volume_max = {3.0f, 5.0f, 5.5f}};
    s.bloom_strength = 0.05f;
    RenderScene sc{.meshes = meshes, .sdf = sdfs, .rect_lights = std::span(&panel, 1),
                   .sun = {.intensity = 0.0f},
                   .environment = {.sky = {0.02f, 0.025f, 0.03f}, .ground = {0.01f, 0.01f, 0.01f}, .exposure = 1.1f}};
    b.run_scene("cornell_area", s, b.make_camera({0, 2.45f, 9.6f}, {0, 2.2f, 0}, 40.0f), sc, warmup, measure);
}

void scene_material_grid(BenchContext& b, int warmup, int measure) {
    std::vector<MeshInstance> meshes;
    MaterialHandle floor_mat = b.renderer->create_material({.base_color = {0.25f, 0.25f, 0.27f, 1}, .roughness = 0.75f});
    meshes.push_back({b.plane, floor_mat, translate({0, -1.5f, 0}) * scale({30, 1, 30})});
    for (int row = 0; row < 5; ++row) {
        const float metallic = static_cast<float>(row) / 4.0f;
        for (int col = 0; col < 5; ++col) {
            const float roughness = 0.04f + 0.96f * (static_cast<float>(col) / 4.0f);
            MaterialHandle m = b.renderer->create_material(
                {.base_color = {0.9f, 0.72f, 0.35f, 1}, .roughness = roughness, .metallic = metallic});
            const float x = (static_cast<float>(col) - 2.0f) * 1.4f;
            const float y = (static_cast<float>(row) - 2.0f) * 1.4f + 2.2f;
            meshes.push_back({b.sphere, m, translate({x, y, 0})});
        }
    }
    RectLight key{.position = {-3, 6, 4}, .right = {1.5f, 0, 0}, .up = {0, 0, 1.5f},
                  .color = {1.0f, 0.95f, 0.9f}, .intensity = 14.0f};
    RenderSettings s;
    s.gi = {.probes_x = 6, .probes_y = 6, .probes_z = 6, .rays_per_probe = 64};
    RenderScene sc{.meshes = meshes, .rect_lights = std::span(&key, 1),
                   .sun = {.direction = {-0.4f, -0.7f, -0.5f}, .intensity = 2.5f},
                   .environment = {.sky = {0.35f, 0.45f, 0.65f}, .ground = {0.12f, 0.12f, 0.14f}, .exposure = 1.0f}};
    b.run_scene("material_grid", s, b.make_camera({0, 2.2f, 10.5f}, {0, 2.2f, 0}, 42.0f), sc, warmup, measure);
}

void scene_thin_wall(BenchContext& b, int warmup, int measure) {
    MaterialHandle white = b.renderer->create_material({.base_color = {0.8f, 0.8f, 0.8f, 1}, .roughness = 0.85f});
    MaterialHandle red = b.renderer->create_material({.base_color = {0.9f, 0.1f, 0.08f, 1}, .roughness = 0.85f});
    // Left half (x < 0): dark sealed room; right half (x > 0): brightly lit room; thin wall at x = 0 (thickness 0.12).
    std::vector<MeshInstance> meshes = {
        {b.cube, white, box_tf({0, -0.1f, 0}, {12, 0.2f, 8})},
        {b.cube, white, box_tf({0, 4.1f, 0}, {12, 0.2f, 8})},
        {b.cube, white, box_tf({0, 2.0f, -4.1f}, {12, 4.2f, 0.2f})},
        {b.cube, white, box_tf({-6.1f, 2.0f, 0}, {0.2f, 4.2f, 8})},
        {b.cube, red, box_tf({6.1f, 2.0f, 0}, {0.2f, 4.2f, 8})},
        {b.cube, white, box_tf({0.0f, 2.0f, 0}, {0.12f, 4.0f, 8.0f})}, // thin divider wall
    };
    RectLight bright{.position = {3.0f, 3.95f, 0}, .right = {1.2f, 0, 0}, .up = {0, 0, 1.2f},
                     .color = {1.0f, 0.95f, 0.85f}, .intensity = 40.0f};
    RenderSettings s;
    s.gi = {.probes_x = 8, .probes_y = 6, .probes_z = 6, .rays_per_probe = 128,
            .volume_min = {-6, 0, -4}, .volume_max = {6, 4, 4}};
    RenderScene sc{.meshes = meshes, .rect_lights = std::span(&bright, 1),
                   .sun = {.intensity = 0.0f},
                   .environment = {.sky = {0.0f, 0.0f, 0.0f}, .ground = {0.0f, 0.0f, 0.0f}, .exposure = 1.0f}};
    b.run_scene("thin_wall", s, b.make_camera({-2.5f, 2.0f, 3.8f}, {-1.0f, 2.0f, -2.0f}, 55.0f), sc, warmup, measure);
}

void scene_high_lights(BenchContext& b, int warmup, int measure) {
    MaterialHandle floor_mat = b.renderer->create_material({.base_color = {0.6f, 0.6f, 0.62f, 1}, .roughness = 0.35f});
    MaterialHandle metal = b.renderer->create_material({.base_color = {0.9f, 0.9f, 0.92f, 1}, .roughness = 0.2f, .metallic = 1.0f});
    std::vector<MeshInstance> meshes = {{b.plane, floor_mat, scale({30, 1, 30})}};
    for (int i = 0; i < 16; ++i) {
        const float x = static_cast<float>(i % 4 - 1.5f) * 3.0f;
        const float z = static_cast<float>(i / 4 - 1.5f) * 3.0f;
        meshes.push_back({b.cube, (i & 1) ? metal : floor_mat, translate({x, 0.75f, z}) * scale({1.2f, 1.5f, 1.2f})});
    }
    std::vector<PointLight> lights;
    for (int i = 0; i < 32; ++i) {
        const float a = static_cast<float>(i) * 0.19635f;
        const float r = 3.0f + static_cast<float>(i % 4) * 2.2f;
        lights.push_back({.position = {std::cos(a) * r, 2.0f + static_cast<float>(i % 3), std::sin(a) * r},
                          .color = {0.5f + 0.5f * std::sin(a), 0.5f + 0.5f * std::cos(a * 1.7f), 0.5f + 0.5f * std::sin(a * 2.3f)},
                          .intensity = 15.0f, .radius = 8.0f});
    }
    std::vector<RectLight> rects = {
        {.position = {-4, 5, -4}, .right = {1, 0, 0}, .up = {0, 0, 1}, .intensity = 10.0f},
        {.position = {4, 5, -4}, .right = {1, 0, 0}, .up = {0, 0, 1}, .intensity = 10.0f},
        {.position = {-4, 5, 4}, .right = {1, 0, 0}, .up = {0, 0, 1}, .intensity = 10.0f},
        {.position = {4, 5, 4}, .right = {1, 0, 0}, .up = {0, 0, 1}, .intensity = 10.0f},
    };
    std::vector<SdfShape> sdfs;
    for (int i = 0; i < 8; ++i) {
        const float a = static_cast<float>(i) * 0.7854f;
        sdfs.push_back({.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::SmoothUnion,
                        .position = {std::cos(a) * 2.5f, 1.2f, std::sin(a) * 2.5f},
                        .size = {0.45f, 0, 0, 0}, .blend = 0.25f, .color = {0.8f, 0.3f, 0.2f, 1}, .roughness = 0.3f});
    }
    RenderSettings s;
    s.gi = {.probes_x = 8, .probes_y = 6, .probes_z = 8, .rays_per_probe = 128};
    RenderScene sc{.meshes = meshes, .sdf = sdfs, .lights = lights, .rect_lights = rects,
                   .sun = {.direction = {-0.3f, -1.0f, -0.4f}, .intensity = 1.5f},
                   .environment = {.sky = {0.06f, 0.08f, 0.12f}, .ground = {0.02f, 0.02f, 0.02f}}};
    const CameraData cam = b.make_camera({0, 8.5f, 14.0f}, {0, 0.5f, 0}, 45.0f);
    s.restir_di = false;
    b.run_scene("high_lights_ris", s, cam, sc, warmup, measure);
    s.restir_di = true;
    b.run_scene("high_lights", s, cam, sc, warmup, measure);
}

void scene_many_lights(BenchContext& b, int warmup, int measure) {
    MaterialHandle floor_mat = b.renderer->create_material({.base_color = {0.65f, 0.65f, 0.68f, 1}, .roughness = 0.45f});
    MaterialHandle metal = b.renderer->create_material({.base_color = {0.9f, 0.9f, 0.92f, 1}, .roughness = 0.25f, .metallic = 1.0f});
    std::vector<MeshInstance> meshes = {{b.plane, floor_mat, scale({40, 1, 40})}};
    for (int i = 0; i < 25; ++i) {
        const float x = static_cast<float>(i % 5 - 2) * 3.5f;
        const float z = static_cast<float>(i / 5 - 2) * 3.5f;
        meshes.push_back({b.cube, (i & 1) ? metal : floor_mat, translate({x, 1.4f, z}) * scale({1.5f, 2.8f, 1.5f})});
    }
    // Interior partition walls create heavy light occlusion (where visibility-aware ReSTIR shines over single-frame RIS).
    meshes.push_back({b.cube, floor_mat, box_tf({-5.0f, 1.6f, 0.0f}, {0.35f, 3.2f, 12.0f})});
    meshes.push_back({b.cube, floor_mat, box_tf({5.0f, 1.6f, 0.0f}, {0.35f, 3.2f, 12.0f})});
    std::vector<PointLight> lights;
    for (int i = 0; i < 128; ++i) {
        const float a = static_cast<float>(i) * 0.3927f;
        const float r = 2.0f + static_cast<float>(i % 8) * 1.6f;
        lights.push_back({.position = {std::cos(a) * r, 1.4f + static_cast<float>(i % 3) * 0.7f, std::sin(a) * r},
                          .color = {0.5f + 0.5f * std::sin(a * 1.1f), 0.5f + 0.5f * std::cos(a * 1.7f),
                                    0.5f + 0.5f * std::sin(a * 2.3f)},
                          .intensity = 4.0f, .radius = 9.0f});
    }
    std::vector<RectLight> rects = {
        {.position = {-6, 6, -6}, .right = {1.2f, 0, 0}, .up = {0, 0, 1.2f}, .intensity = 6.0f},
        {.position = {6, 6, -6}, .right = {1.2f, 0, 0}, .up = {0, 0, 1.2f}, .intensity = 6.0f},
        {.position = {-6, 6, 6}, .right = {1.2f, 0, 0}, .up = {0, 0, 1.2f}, .intensity = 6.0f},
        {.position = {6, 6, 6}, .right = {1.2f, 0, 0}, .up = {0, 0, 1.2f}, .intensity = 6.0f},
    };
    RenderSettings s;
    s.gi = {.probes_x = 8, .probes_y = 6, .probes_z = 8, .rays_per_probe = 64};
    RenderScene sc{.meshes = meshes, .lights = lights, .rect_lights = rects,
                   .sun = {.intensity = 0.0f},
                   .environment = {.sky = {0.03f, 0.04f, 0.06f}, .ground = {0.01f, 0.01f, 0.01f}}};
    const CameraData cam = b.make_camera({0, 10.0f, 16.0f}, {0, 0.5f, 0}, 45.0f);
    // Measure raw direct-light noise (denoise_iterations = 0, max_history = 1 for pure spatial+ReSTIR comparison,
    // and max_history = 16 for temporal accumulation) against a 64-frame converged RIS reference.
    auto eval_raw = [&](bool restir, float max_hist, int frames) {
        RenderSettings sr = s;
        sr.denoise_iterations = 0;
        sr.taa_blend = 1.0f;
        sr.max_history = max_hist;
        sr.restir_di = restir;
        b.renderer->set_settings(sr);
        b.renderer->reset_history();
        for (int i = 0; i < frames; ++i) {
            auto cmd = b.device->begin_commands();
            b.renderer->render(cmd, b.target, cam, sc);
            b.device->wait(b.device->submit(cmd));
        }
        return b.capture_target();
    };
    s.restir_di = false;
    b.run_scene("many_lights_ris", s, cam, sc, warmup, measure);
    s.restir_di = true;
    b.run_scene("many_lights_restir", s, cam, sc, warmup, measure);
    const auto ref = eval_raw(false, 64.0f, 64);
    const auto raw_ris = eval_raw(false, 1.0f, 16);
    const auto raw_restir = eval_raw(true, 1.0f, 16);
    auto stats_vs_ref = [&](const std::vector<std::uint8_t>& img, const char* tag) {
        double sq = 0.0, mean = 0.0, ref_m = 0.0;
        int n = 0;
        for (std::uint32_t y = b.height / 4; y < 7 * b.height / 8; ++y) {
            for (std::uint32_t x = b.width / 6; x < 5 * b.width / 6; ++x) {
                const std::size_t i = (std::size_t{y} * b.width + x) * 4;
                for (int c = 0; c < 3; ++c) {
                    const double d = static_cast<double>(img[i + c]) - static_cast<double>(ref[i + c]);
                    sq += d * d;
                    mean += img[i + c];
                    ref_m += ref[i + c];
                }
                ++n;
            }
        }
        std::printf("  [%s 1-frame denoiser off] mean=%.2f (ref=%.2f) | RMSE_vs_ref=%.3f\n",
                    tag, mean / (3.0 * n), ref_m / (3.0 * n), std::sqrt(sq / (3.0 * n)));
    };
    stats_vs_ref(raw_ris, "many_lights_ris");
    stats_vs_ref(raw_restir, "many_lights_restir");
}

void scene_moving_mirror(BenchContext& b) {
    MaterialHandle mirror = b.renderer->create_material({.base_color = {0.98f, 0.98f, 0.98f, 1}, .roughness = 0.02f, .metallic = 1.0f});
    MaterialHandle dark = b.renderer->create_material({.base_color = {0.06f, 0.06f, 0.08f, 1}, .roughness = 0.8f});
    MaterialHandle glow_a = b.renderer->create_material({.base_color = {0, 0, 0, 1}, .emissive = {12.0f, 2.0f, 1.0f}});
    MaterialHandle glow_b = b.renderer->create_material({.base_color = {0, 0, 0, 1}, .emissive = {1.0f, 8.0f, 12.0f}});
    std::vector<MeshInstance> meshes = {
        {b.plane, mirror, scale({30, 1, 30})},
        {b.sphere, mirror, translate({0, 1.2f, -1.0f}) * scale({1.6f, 1.6f, 1.6f})},
        {b.cube, dark, box_tf({0, 3.0f, -6.0f}, {16, 6, 0.4f})},
        {b.cube, glow_a, box_tf({-2.4f, 2.6f, -5.5f}, {0.45f, 3.6f, 0.3f})},
        {b.cube, glow_b, box_tf({0.0f, 2.6f, -5.5f}, {0.45f, 3.6f, 0.3f})},
        {b.cube, glow_a, box_tf({2.4f, 2.6f, -5.5f}, {0.45f, 3.6f, 0.3f})},
    };
    RectLight panel{.position = {0, 5.2f, -2.5f}, .right = {1.6f, 0, 0}, .up = {0, 0, 0.6f}, .intensity = 16.0f};
    RenderSettings s;
    s.gi = {.probes_x = 6, .probes_y = 5, .probes_z = 6, .rays_per_probe = 64};
    s.denoise_iterations = 0;
    s.taa_blend = 1.0f;
    RenderScene sc{.meshes = meshes, .rect_lights = std::span(&panel, 1),
                   .sun = {.intensity = 0.0f}, .environment = {.sky = {0.02f, 0.02f, 0.03f}, .ground = {0.01f, 0.01f, 0.01f}}};
    b.renderer->set_settings(s);
    b.renderer->reset_history();
    CameraData last_cam{};
    double gpu_sum = 0.0;
    int gpu_n = 0;
    for (int f = 0; f < 16; ++f) {
        const float x = -3.0f + static_cast<float>(f) * 0.4f;
        last_cam = b.make_camera({x, 2.2f, 6.0f}, {0, 0.8f, -2.0f}, 45.0f);
        auto cmd = b.device->begin_commands();
        b.renderer->render(cmd, b.target, last_cam, sc);
        b.device->wait(b.device->submit(cmd));
        if (f >= 8 && b.renderer->gpu_frame_ms() > 0.0f) { gpu_sum += b.renderer->gpu_frame_ms(); ++gpu_n; }
    }
    const auto moving_img = b.capture_target();
    if (!b.out_dir.empty()) write_ppm(b.out_dir + "/moving_mirror.ppm", moving_img, b.width, b.height);
    // Render 16 stationary frames at the final camera pose so both frames share the exact same Halton(2,3) jitter phase.
    b.renderer->reset_history();
    for (int f = 0; f < 16; ++f) {
        auto cmd = b.device->begin_commands();
        b.renderer->render(cmd, b.target, last_cam, sc);
        b.device->wait(b.device->submit(cmd));
    }
    const auto ref_img = b.capture_target();
    if (!b.out_dir.empty()) write_ppm(b.out_dir + "/moving_mirror_ref.ppm", ref_img, b.width, b.height);
    double sq = 0.0;
    int cnt = 0;
    for (std::uint32_t y = b.height / 2; y < b.height - 16; ++y) {
        for (std::uint32_t x = b.width / 6; x < 5 * b.width / 6; ++x) {
            const std::size_t i = (std::size_t{y} * b.width + x) * 4;
            for (int c = 0; c < 3; ++c) {
                const double d = static_cast<double>(moving_img[i + c]) - static_cast<double>(ref_img[i + c]);
                sq += d * d;
            }
            ++cnt;
        }
    }
    std::printf("\n[scene: moving_mirror] gpu_frame_ms=%.3f ms | mirror_motion_rmse=%.3f (lower = sharper, less ghosting)\n",
                gpu_n > 0 ? gpu_sum / gpu_n : 0.0, std::sqrt(sq / (3.0 * cnt)));
}

void scene_moving_camera_stability(BenchContext& b) {
    MaterialHandle white = b.renderer->create_material({.base_color = {0.8f, 0.8f, 0.78f, 1}, .roughness = 0.75f});
    MaterialHandle red = b.renderer->create_material({.base_color = {0.85f, 0.1f, 0.08f, 1}, .roughness = 0.7f});
    std::vector<MeshInstance> meshes = {
        {b.plane, white, scale({30, 1, 30})},
        {b.cube, red, box_tf({0, 1.5f, -4}, {3, 3, 0.5f})},
        {b.cube, white, box_tf({-2.5f, 1.0f, -2}, {1.2f, 2.0f, 1.2f})},
        {b.cube, white, box_tf({2.5f, 1.0f, -2}, {1.2f, 2.0f, 1.2f})},
    };
    RectLight panel{.position = {0, 4.5f, -2}, .right = {1.0f, 0, 0}, .up = {0, 0, 1.0f}, .intensity = 18.0f};
    RenderSettings s;
    s.gi = {.probes_x = 6, .probes_y = 5, .probes_z = 6, .rays_per_probe = 64};
    s.denoise_iterations = 0; // isolate temporal accumulation stability
    RenderScene sc{.meshes = meshes, .rect_lights = std::span(&panel, 1),
                   .sun = {.intensity = 0.0f},
                   .environment = {.sky = {0.02f, 0.02f, 0.03f}, .ground = {0.01f, 0.01f, 0.01f}}};
    b.renderer->set_settings(s);
    b.renderer->reset_history();
    // Translate camera forward smoothly at 0.45 m/frame (27 m/s at 60 fps): triggers depth-rejection bug if pg.w vs geo.w is wrong!
    std::vector<std::uint8_t> prev_img;
    double mean_step_diff = 0.0;
    int diffs = 0;
    for (int f = 0; f < 20; ++f) {
        const float z = 12.0f - static_cast<float>(f) * 0.45f;
        CameraData cam = b.make_camera({0, 2.0f, z}, {0, 1.5f, -4.0f}, 45.0f);
        auto cmd = b.device->begin_commands();
        b.renderer->render(cmd, b.target, cam, sc);
        b.device->wait(b.device->submit(cmd));
        if (f >= 15) {
            auto cur = b.capture_target();
            if (!prev_img.empty()) {
                // Measure spatial noise (high-frequency neighbor diff on the center wall)
                double hf = 0.0;
                int n = 0;
                for (std::uint32_t y = b.height / 3; y < 2 * b.height / 3; ++y) {
                    for (std::uint32_t x = b.width / 3 + 1; x < 2 * b.width / 3; ++x) {
                        const std::size_t i0 = (std::size_t{y} * b.width + x) * 4;
                        const std::size_t i1 = (std::size_t{y} * b.width + x - 1) * 4;
                        for (int c = 0; c < 3; ++c) hf += std::abs(int{cur[i0 + c]} - int{cur[i1 + c]});
                        ++n;
                    }
                }
                mean_step_diff += hf / (3.0 * n);
                ++diffs;
            }
            prev_img = std::move(cur);
            if (f == 19 && !b.out_dir.empty()) write_ppm(b.out_dir + "/moving_camera.ppm", prev_img, b.width, b.height);
        }
    }
    std::printf("\n[scene: moving_camera (denoise_iter=0)] spatial_hf_noise=%.3f (lower = better temporal accumulation)\n",
                diffs > 0 ? mean_step_diff / diffs : 0.0);
}

void scene_open_world_scale(BenchContext& b, int warmup, int measure) {
    MaterialHandle ground = b.renderer->create_material({.base_color = {0.35f, 0.45f, 0.3f, 1}, .roughness = 0.85f});
    MaterialHandle tower = b.renderer->create_material({.base_color = {0.75f, 0.72f, 0.68f, 1}, .roughness = 0.6f});
    // 2000m world extent -> spacing = 250m -> max_dist = 1.5*sqrt(3)*250 = 650m -> d^2 = 422,500 (> 65504 FP16 max!)
    std::vector<MeshInstance> meshes = {
        {b.plane, ground, translate({0, 0, 0}) * scale({2000, 1, 2000})},
        {b.cube, tower, translate({0, 25, 0}) * scale({16, 50, 16})},
        {b.cube, tower, translate({400, 30, -300}) * scale({30, 60, 30})},
        {b.cube, tower, translate({-500, 40, -600}) * scale({40, 80, 40})},
    };
    RenderSettings s;
    s.gi = {.probes_x = 8, .probes_y = 4, .probes_z = 8, .rays_per_probe = 64};
    s.max_ray_distance = 3000.0f;
    RenderScene sc{.meshes = meshes,
                   .sun = {.direction = {-0.4f, -0.55f, -0.35f}, .intensity = 3.0f},
                   .environment = {.sky = {0.45f, 0.6f, 0.85f}, .ground = {0.18f, 0.16f, 0.14f}}};
    b.run_scene("open_world_scale", s, b.make_camera({120, 45, 180}, {0, 15, 0}, 50.0f, 0.2f, 3000.0f), sc, warmup, measure);
}

} // namespace

int main(int argc, char** argv) {
    std::uint32_t w = 1280, h = 720;
    int warmup = 24, measure = 16;
    std::string out_dir;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) w = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) h = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) warmup = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--measure") == 0 && i + 1 < argc) measure = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) out_dir = argv[++i];
    }
    BenchContext b;
    if (!b.init(w, h, out_dir)) {
        std::fprintf(stderr, "No Vulkan GPU available\n");
        return 1;
    }
    const gpu::AdapterInfo info = b.device->adapter_info();
    std::printf("GPU: %.*s | driver=%.*s | rt=%d | timestamps=%d\n",
                static_cast<int>(info.name.size()), info.name.data(),
                static_cast<int>(info.driver_name.size()), info.driver_name.data(),
                b.renderer->ray_tracing() ? 1 : 0,
                b.device->supports(gpu::Feature::Timestamps) ? 1 : 0);

    scene_cornell_area(b, warmup, measure);
    scene_material_grid(b, warmup, measure);
    scene_thin_wall(b, warmup, measure);
    scene_high_lights(b, warmup, measure);
    scene_many_lights(b, warmup, measure);
    scene_moving_camera_stability(b);
    scene_moving_mirror(b);
    scene_open_world_scale(b, warmup, measure);
    return 0;
}
