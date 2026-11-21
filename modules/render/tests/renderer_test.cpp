#include <astral/gpu/device.hpp>
#include <astral/render/renderer.hpp>
#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <vector>

using namespace astral;

namespace {

struct Fixture {
    std::unique_ptr<gpu::Device> device;
    std::unique_ptr<Renderer> renderer;
    gpu::TextureHandle target;
    gpu::BufferHandle readback;
    static constexpr std::uint32_t size = 64;

    Fixture() {
        device = gpu::Device::create({.app_name = "render_tests", .validation = true});
        if (!device) return;
        renderer = std::make_unique<Renderer>(*device, gpu::Format::RGBA8Unorm);
        target = device->create_texture({.width = size, .height = size, .format = gpu::Format::RGBA8Unorm,
                                         .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::TransferSrc});
        readback = device->create_buffer({.size = size * size * 4, .usage = gpu::BufferUsage::TransferDst,
                                          .memory = gpu::MemoryUsage::GpuToCpu});
    }

    std::vector<std::uint32_t> draw(const std::vector<DrawItem>& items) {
        CameraData camera;
        camera.position = {0, 0, 5};
        camera.view = look_at(camera.position, {0, 0, 0}, {0, 1, 0});
        camera.projection = perspective(radians(60), 1.0f, 0.1f, 100.0f);
        auto cmd = device->begin_commands();
        renderer->render(cmd, target, camera, items,
                         {.direction = {0, 0, -1}, .color = {1, 1, 1}, .intensity = 1, .ambient = 0});
        cmd.transition(target, gpu::ResourceState::TransferSrc);
        cmd.copy_texture_to_buffer(target, readback);
        device->wait(device->submit(cmd));
        std::vector<std::uint32_t> pixels(size * size);
        device->read_buffer(readback, pixels.data(), pixels.size() * 4);
        return pixels;
    }
};

std::uint8_t red(std::uint32_t p) { return static_cast<std::uint8_t>(p & 0xFF); }
std::uint8_t green(std::uint32_t p) { return static_cast<std::uint8_t>((p >> 8) & 0xFF); }

} // namespace

TEST_CASE("primitive meshes have consistent geometry") {
    const MeshData cube = make_cube();
    CHECK(cube.vertices.size() == 24);
    CHECK(cube.indices.size() == 36);
    const MeshData plane = make_plane(10.0f);
    CHECK(plane.indices.size() == 6);
    const MeshData sphere = make_sphere(16, 8);
    CHECK(sphere.vertices.size() > 0);
    for (auto i : sphere.indices) CHECK(i < sphere.vertices.size());
}

TEST_CASE("a lit cube in front of the camera is drawn over the clear color") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    REQUIRE(cube.valid());
    auto pixels = f.draw({{.mesh = cube, .transform = Mat4::identity(), .color = {1, 0, 0, 1}}});
    const std::uint32_t center = pixels[(Fixture::size / 2) * Fixture::size + Fixture::size / 2];
    const std::uint32_t corner = pixels[0];
    CHECK(red(center) > 200);   // lit red face facing the light
    CHECK(green(center) < 30);
    CHECK(red(corner) < 60);    // clear color is dark
}

TEST_CASE("depth test keeps the nearer object in front") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    auto pixels = f.draw({
        {.mesh = cube, .transform = translate({0, 0, 1}), .color = {0, 1, 0, 1}},   // near, green
        {.mesh = cube, .transform = translate({0, 0, -1}), .color = {1, 0, 0, 1}},  // far, red (drawn after)
    });
    const std::uint32_t center = pixels[(Fixture::size / 2) * Fixture::size + Fixture::size / 2];
    CHECK(green(center) > 200);
    CHECK(red(center) < 30);
}

TEST_CASE("many instances of one mesh are drawn") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    std::vector<DrawItem> items;
    for (int i = 0; i < 1000; ++i) {
        items.push_back({.mesh = cube, .transform = translate({0, 0, -static_cast<float>(i)}),
                         .color = {0, 0, 1, 1}});
    }
    auto pixels = f.draw(items);
    const std::uint32_t center = pixels[(Fixture::size / 2) * Fixture::size + Fixture::size / 2];
    CHECK(((center >> 16) & 0xFF) > 200); // blue
    CHECK(f.renderer->stats().instances == 1000);
    CHECK(f.renderer->stats().draw_calls == 1); // instanced: one call per mesh
}

TEST_CASE("draw items with destroyed meshes are skipped") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    f.renderer->destroy(cube);
    auto pixels = f.draw({{.mesh = cube, .transform = Mat4::identity(), .color = {1, 0, 0, 1}}});
    CHECK(red(pixels[(Fixture::size / 2) * Fixture::size + Fixture::size / 2]) < 60);
}
