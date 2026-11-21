// model_viewer — shows a glTF/GLB model in a small lit studio, with hot reload: re-export from
// Blender and the model updates in place.
//
//   model_viewer path/to/model.glb                 window (App + ModelRef)
//   model_viewer path/to/model.glb --capture out.ppm [--frames 48] [--width 1280] [--height 720]
//                                                  headless still image
// A procedural test model: python3 tools/make_test_model.py crate.glb
#include <astral/astral.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace astral;

namespace {

struct Studio {
    Vec3 eye{2.3f, 1.7f, 2.8f};
    Vec3 target{0, 0.55f, 0};
    RectLight key{.position = {-1.5f, 3.0f, 1.5f}, .right = {0.6f, 0, 0.6f}, .up = {-0.35f, 0.5f, 0.35f},
                  .color = {1.0f, 0.95f, 0.88f}, .intensity = 14.0f};
    DirectionalLight sun{.direction = {0.4f, -1.0f, -0.6f}, .color = {1.0f, 0.97f, 0.92f}, .intensity = 1.2f, .angular_radius = 0.02f};
    Environment environment{.sky = {0.35f, 0.42f, 0.52f}, .ground = {0.12f, 0.11f, 0.1f}, .exposure = 1.0f};
};

int capture(const std::filesystem::path& file, const char* out, std::uint32_t frames, std::uint32_t width, std::uint32_t height) {
    auto device = gpu::Device::create({.app_name = "model_viewer"});
    if (!device) return std::fprintf(stderr, "no Vulkan device\n"), 1;
    RenderSettings settings;
    settings.gi = {.probes_x = 8, .probes_y = 5, .probes_z = 8, .rays_per_probe = 96,
                   .volume_min = {-4, 0, -4}, .volume_max = {4, 3, 4}};
    Renderer renderer(*device, gpu::Format::RGBA8Unorm, settings);
    AssetManager assets({.root = file.parent_path(), .renderer = &renderer, .hot_reload = false});
    const AssetId id = assets.load_model(file.filename().string());
    assets.wait_all();
    const Model* model = assets.model(id);
    if (!model) return std::fprintf(stderr, "%.*s\n", static_cast<int>(assets.error(id).size()), assets.error(id).data()), 1;

    // Flatten the node tree into instances (the App does the same through entities).
    std::vector<MeshInstance> items;
    struct Item {
        std::uint32_t node;
        Mat4 parent;
    };
    std::vector<Item> stack;
    for (std::uint32_t r : model->roots) stack.push_back({r, Mat4::identity()});
    while (!stack.empty()) {
        const Item it = stack.back();
        stack.pop_back();
        const Model::Node& n = model->nodes[it.node];
        const Mat4 world = it.parent * compose(n.translation, n.rotation, n.scale);
        for (const auto& p : n.pieces) items.push_back({.mesh = p.mesh, .material = p.material, .transform = world});
        for (std::uint32_t c : n.children) stack.push_back({c, world});
    }
    const MeshHandle plane = renderer.create_mesh(make_plane(1.0f));
    items.push_back({.mesh = plane, .material = renderer.create_material({.base_color = {0.5f, 0.5f, 0.52f, 1}, .roughness = 0.55f}),
                     .transform = translate({0, -0.5f, 0}) * scale({12, 1, 12})});

    const Studio studio;
    CameraData camera;
    camera.position = studio.eye;
    camera.view = look_at(studio.eye, studio.target, {0, 1, 0});
    camera.projection = perspective(radians(40.0f), static_cast<float>(width) / static_cast<float>(height), 0.05f, 100.0f);
    const RenderScene scene{.meshes = items, .rect_lights = std::span(&studio.key, 1), .sun = studio.sun,
                            .environment = studio.environment};
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
    }
    std::vector<std::uint8_t> rgba(std::size_t{width} * height * 4);
    device->read_buffer(readback, rgba.data(), rgba.size());
    std::FILE* f = std::fopen(out, "wb");
    if (!f) return 1;
    std::fprintf(f, "P6\n%u %u\n255\n", width, height);
    for (std::size_t p = 0; p < std::size_t{width} * height; ++p) std::fwrite(&rgba[p * 4], 1, 3, f);
    std::fclose(f);
    std::printf("wrote %s (%zu nodes, ray tracing: %s)\n", out, model->nodes.size(), renderer.ray_tracing() ? "yes" : "no");
    return 0;
}

class Viewer final : public Game {
public:
    explicit Viewer(std::string file) : file_(std::move(file)) {}
    void on_start(Context& ctx) override {
        const Studio studio;
        ctx.world.create(Transform{}, ModelRef{file_});
        ctx.world.create(Transform{.position = {0, -0.5f, 0}, .scale = {12, 1, 12}},
                         MeshRenderer{ctx.meshes.plane, ctx.create_material({.base_color = {0.5f, 0.5f, 0.52f, 1}, .roughness = 0.55f})});
        ctx.world.create(Transform{.position = studio.key.position}, RectLight{studio.key});
        ctx.world.create(Transform{.position = studio.eye, .rotation = rotation_of(inverse(look_at(studio.eye, studio.target, {0, 1, 0})))},
                         Camera{.fov_y = radians(40.0f), .near_plane = 0.05f});
        ctx.sun = studio.sun;
        ctx.environment = studio.environment;
        ctx.log.info("viewing {} — edit and re-export it: it reloads", file_);
    }
    void on_update(Context& ctx) override {
        if (ctx.input.pressed(Key::Escape)) ctx.quit();
    }

private:
    std::string file_;
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: model_viewer <model.gltf|glb> [--capture out.ppm --frames N --width W --height H]\n");
        return 1;
    }
    const std::filesystem::path file = std::filesystem::absolute(argv[1]);
    const char* out = nullptr;
    std::uint32_t frames = 48, width = 1280, height = 720;
    for (int i = 2; i < argc; ++i) {
        auto next = [&] { return i + 1 < argc ? argv[++i] : "0"; };
        if (!std::strcmp(argv[i], "--capture")) out = next();
        else if (!std::strcmp(argv[i], "--frames")) frames = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--width")) width = static_cast<std::uint32_t>(std::atoi(next()));
        else if (!std::strcmp(argv[i], "--height")) height = static_cast<std::uint32_t>(std::atoi(next()));
    }
    if (out) return capture(file, out, frames, width, height);
    const std::string root = file.parent_path().string();
    App app({.title = "Astral — model viewer", .asset_root = root});
    return app.run<Viewer>(file.filename().string());
}
