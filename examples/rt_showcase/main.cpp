// rt_showcase — the ray-traced renderer (ADR-0011) in a closed "RGB room":
// red / green / blue walls, a mirror-chrome sphere in the middle, glossy and rough boxes, a gold
// SDF torus, and a single ceiling panel light. Everything you see beyond the panel's direct light
// — colour bleeding, soft contact shadows, the room in the sphere — comes from ray tracing.
//
//   rt_showcase                         window (needs a ray-tracing GPU; falls back to raster)
//   rt_showcase --capture out.ppm [--frames 64] [--width 1280] [--height 720] [--view 0..2]
//               [--debug-view 0..4] [--raster]
//                                       headless: renders N frames and writes the final image
//                                       (debug views: albedo, normals, diffuse light, specular)
#include <astral/astral.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace astral;

namespace {

enum class Shape { Cube, Sphere };

struct Object {
    Shape shape;
    Mat4 transform;
    Vec4 color;
    float roughness;
    float metallic;
};

struct Room {
    std::vector<Object> objects;
    std::vector<SdfShape> sdf;
    RectLight panel;
};

Mat4 box(Vec3 center, Vec3 size, float yaw_degrees = 0.0f) {
    return translate(center) * rotate(Quat::from_axis_angle({0, 1, 0}, radians(yaw_degrees))) * scale(size);
}

Room build_room() {
    Room r;
    // Inner size and wall thickness. The room runs from the back wall (z = -3.5) past the camera's
    // field of view at the open front (z = 5.5), like a photo studio set.
    const float W = 6.0f, H = 5.0f, D = 9.0f, t = 0.2f, zc = 1.0f;
    const Vec4 white{0.78f, 0.78f, 0.76f, 1}, red{0.82f, 0.06f, 0.05f, 1}, green{0.08f, 0.62f, 0.12f, 1},
        blue{0.06f, 0.18f, 0.85f, 1};
    // Room shell: floor, ceiling, back wall, red left wall, green right wall.
    r.objects.push_back({Shape::Cube, box({0, -t / 2, zc}, {W + 2 * t, t, D}), white, 0.35f, 0});
    r.objects.push_back({Shape::Cube, box({0, H + t / 2, zc}, {W + 2 * t, t, D}), white, 0.9f, 0});
    r.objects.push_back({Shape::Cube, box({0, H / 2, zc - D / 2 - t / 2}, {W + 2 * t, H, t}), white, 0.9f, 0});
    r.objects.push_back({Shape::Cube, box({-W / 2 - t / 2, H / 2, zc}, {t, H, D}), red, 0.85f, 0});
    r.objects.push_back({Shape::Cube, box({W / 2 + t / 2, H / 2, zc}, {t, H, D}), green, 0.85f, 0});

    // Mirror-chrome sphere in the middle, on a low white plinth.
    r.objects.push_back({Shape::Cube, box({0, 0.25f, -0.4f}, {1.6f, 0.5f, 1.6f}, 0), white, 0.6f, 0});
    r.objects.push_back({Shape::Sphere, translate({0, 1.45f, -0.4f}) * scale({1.9f, 1.9f, 1.9f}),
                         {0.97f, 0.97f, 0.97f, 1}, 0.02f, 1.0f});
    // A tall glossy blue box (back left) and a short rough white box (front right).
    r.objects.push_back({Shape::Cube, box({-1.75f, 1.6f, -2.0f}, {1.3f, 3.2f, 1.3f}, 22), blue, 0.18f, 0});
    r.objects.push_back({Shape::Cube, box({1.8f, 0.6f, 0.9f}, {1.2f, 1.2f, 1.2f}, -18), white, 0.7f, 0});
    // Brushed copper cube on top of it.
    r.objects.push_back({Shape::Cube, box({1.75f, 1.55f, 0.85f}, {0.6f, 0.6f, 0.6f}, 30), {0.95f, 0.64f, 0.54f, 1}, 0.32f, 1.0f});

    // Gold SDF torus leaning on the floor (raymarched, but lit, shadowed and reflected like a mesh).
    r.sdf.push_back({.primitive = SdfPrimitive::Torus, .operation = SdfOperation::Union, .position = {-0.9f, 0.32f, 1.6f},
                     .rotation = Quat::from_axis_angle({1, 0, 0.3f}, radians(70)), .size = {0.45f, 0.14f, 0, 0},
                     .color = {1.0f, 0.78f, 0.34f, 1}, .roughness = 0.22f, .metallic = 1.0f});
    // A small rubbery red SDF capsule pair melting together (smooth union).
    r.sdf.push_back({.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::Union, .position = {0.65f, 0.28f, 2.1f},
                     .size = {0.28f, 0, 0, 0}, .color = {0.85f, 0.1f, 0.08f, 1}, .roughness = 0.45f});
    r.sdf.push_back({.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::SmoothUnion, .position = {1.05f, 0.22f, 2.25f},
                     .size = {0.22f, 0, 0, 0}, .blend = 0.18f, .color = {0.95f, 0.85f, 0.2f, 1}, .roughness = 0.45f});

    // Ceiling panel light, shining down (cross(right, up) = -y).
    r.panel = {.position = {0, H - 0.01f, -0.3f}, .right = {0.85f, 0, 0}, .up = {0, 0, 0.85f},
               .color = {1.0f, 0.93f, 0.84f}, .intensity = 18.0f};
    return r;
}

struct View {
    Vec3 eye, target;
    float fov;
};
const View views[] = {
    {{0, 2.45f, 9.6f}, {0, 2.2f, 0}, 40},
    {{2.2f, 1.6f, 4.6f}, {-0.2f, 1.3f, -0.6f}, 50},
    {{0, 1.5f, 3.4f}, {0, 1.45f, -0.4f}, 38},
};

RenderSettings showcase_settings() {
    RenderSettings s;
    s.gi = {.probes_x = 8, .probes_y = 7, .probes_z = 10, .rays_per_probe = 128,
            .volume_min = {-3.0f, 0.0f, -3.5f}, .volume_max = {3.0f, 5.0f, 5.5f}};
    s.bloom_strength = 0.05f;
    return s;
}

Environment showcase_environment() { return {.sky = {0.02f, 0.025f, 0.03f}, .ground = {0.01f, 0.01f, 0.01f}, .exposure = 1.1f}; }

// --- Headless capture ------------------------------------------------------------------------

int capture(const char* path, std::uint32_t frames, std::uint32_t width, std::uint32_t height, int view_index,
            DebugView debug_view, bool raster) {
    auto device = gpu::Device::create({.app_name = "rt_showcase"});
    if (!device) {
        std::fprintf(stderr, "no Vulkan device\n");
        return 1;
    }
    RenderSettings settings = showcase_settings();
    settings.debug_view = debug_view;
    settings.ray_tracing = !raster;
    Renderer renderer(*device, gpu::Format::RGBA8Unorm, settings);
    std::printf("device: %.*s, ray tracing: %s\n", static_cast<int>(device->adapter_name().size()),
                device->adapter_name().data(), renderer.ray_tracing() ? "yes" : "no (raster fallback)");
    const MeshHandle cube = renderer.create_mesh(make_cube());
    const MeshHandle sphere = renderer.create_mesh(make_sphere(96, 64));
    const Room room = build_room();
    std::vector<DrawItem> items;
    for (const Object& o : room.objects) {
        items.push_back({.mesh = o.shape == Shape::Cube ? cube : sphere, .transform = o.transform, .color = o.color,
                         .roughness = o.roughness, .metallic = o.metallic});
    }
    const View& v = views[view_index];
    CameraData camera;
    camera.position = v.eye;
    camera.view = look_at(v.eye, v.target, {0, 1, 0});
    camera.projection = perspective(radians(v.fov), static_cast<float>(width) / static_cast<float>(height), 0.05f, 100.0f);
    const RenderScene scene{.meshes = items, .sdf = room.sdf, .rect_lights = std::span(&room.panel, 1),
                            .sun = {.intensity = 0.0f}, .environment = showcase_environment()};

    auto target = device->create_texture({.width = width, .height = height, .format = gpu::Format::RGBA8Unorm,
                                          .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::TransferSrc});
    auto readback = device->create_buffer({.size = std::uint64_t{width} * height * 4, .usage = gpu::BufferUsage::TransferDst,
                                           .memory = gpu::MemoryUsage::GpuToCpu});
    for (std::uint32_t i = 0; i < frames; ++i) {
        auto cmd = device->begin_commands();
        renderer.render(cmd, target, camera, scene);
        if (i + 1 == frames) {
            cmd.transition(target, gpu::ResourceState::TransferSrc);
            cmd.copy_texture_to_buffer(target, readback);
        }
        device->wait(device->submit(cmd));
        if ((i + 1) % 8 == 0) std::printf("frame %u/%u\n", i + 1, frames);
    }
    std::vector<std::uint8_t> rgba(std::size_t{width} * height * 4);
    device->read_buffer(readback, rgba.data(), rgba.size());
    std::FILE* file = std::fopen(path, "wb");
    if (!file) return 1;
    std::fprintf(file, "P6\n%u %u\n255\n", width, height);
    // Vulkan's y axis points down the image: row 0 is the top already.
    for (std::size_t p = 0; p < std::size_t{width} * height; ++p) std::fwrite(&rgba[p * 4], 1, 3, file);
    std::fclose(file);
    const RenderStats& st = renderer.stats();
    std::printf("wrote %s (%u passes, %u GI probes)\n", path, st.passes, st.gi_probes);
    return 0;
}

// --- Windowed game -------------------------------------------------------------------------

class ShowcaseGame final : public Game {
public:
    void on_start(Context& ctx) override {
        const Room room = build_room();
        for (const Object& o : room.objects) {
            ctx.world.create(WorldTransform{o.transform},
                             MeshRenderer{o.shape == Shape::Cube ? ctx.meshes.cube : sphere(ctx), o.color, o.roughness, o.metallic});
        }
        for (std::size_t i = 0; i < room.sdf.size(); ++i) {
            const SdfShape& s = room.sdf[i];
            ctx.world.create(Transform{.position = s.position, .rotation = s.rotation}, Sdf{.shape = s, .order = static_cast<std::int32_t>(i)});
        }
        ctx.world.create(room.panel);
        const View& v = views[0];
        ctx.world.create(Transform{.position = v.eye, .rotation = rotation_of(inverse(look_at(v.eye, v.target, {0, 1, 0})))},
                         Camera{.fov_y = radians(v.fov), .near_plane = 0.05f});
        ctx.sun = {.intensity = 0.0f};
        ctx.environment = showcase_environment();
    }
    void on_update(Context& ctx) override {
        if (ctx.input.pressed(Key::Escape)) ctx.quit();
    }

private:
    MeshHandle sphere(Context& ctx) {
        if (!smooth_sphere_.valid()) smooth_sphere_ = ctx.renderer ? ctx.renderer->create_mesh(make_sphere(96, 64)) : ctx.meshes.sphere;
        return smooth_sphere_;
    }
    MeshHandle smooth_sphere_{};
};

} // namespace

int main(int argc, char** argv) {
    const char* out = nullptr;
    std::uint32_t frames = 64, width = 1280, height = 720;
    int view = 0;
    DebugView debug_view = DebugView::None;
    bool raster = false;
    for (int i = 1; i < argc; ++i) {
        auto next = [&] { return i + 1 < argc ? argv[++i] : "0"; };
        if (!std::strcmp(argv[i], "--capture")) out = next();
        else if (!std::strcmp(argv[i], "--frames")) frames = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--width")) width = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--height")) height = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--view")) view = std::atoi(next()) % 3;
        else if (!std::strcmp(argv[i], "--debug-view")) debug_view = static_cast<DebugView>(std::atoi(next()) % 5);
        else if (!std::strcmp(argv[i], "--raster")) raster = true;
    }
    if (out) return capture(out, frames, width, height, view, debug_view, raster);
    App app({.title = "Astral — RT showcase", .render = showcase_settings()});
    return app.run<ShowcaseGame>();
}
