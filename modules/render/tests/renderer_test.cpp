#include "gpu_test_support.hpp"

#include <astral/render/renderer.hpp>

#include <cstdint>
#include <memory>
#include <vector>

using namespace astral;

namespace {

constexpr std::uint32_t size = 64;

struct Rgb {
    int r, g, b;
};
Rgb rgb(std::uint32_t p) {
    return {static_cast<int>(p & 0xFF), static_cast<int>((p >> 8) & 0xFF), static_cast<int>((p >> 16) & 0xFF)};
}
int luminance(std::uint32_t p) {
    const Rgb c = rgb(p);
    return c.r + c.g + c.b;
}

struct Fixture {
    test::TestDevice device;
    std::unique_ptr<Renderer> renderer;
    gpu::TextureHandle target;
    gpu::BufferHandle readback;
    CameraData camera;

    Fixture() {
        if (!device) return;
        renderer = std::make_unique<Renderer>(*device, gpu::Format::RGBA8Unorm, RenderSettings{.shadow_map_size = 512});
        target = device->create_texture({.width = size, .height = size, .format = gpu::Format::RGBA8Unorm,
                                         .usage = gpu::TextureUsage::ColorTarget | gpu::TextureUsage::TransferSrc});
        readback = device->create_buffer({.size = size * size * 4, .usage = gpu::BufferUsage::TransferDst,
                                          .memory = gpu::MemoryUsage::GpuToCpu});
        look_from({0, 0, 5}, {0, 0, 0});
    }
    void look_from(Vec3 eye, Vec3 at) {
        camera.position = eye;
        camera.view = look_at(eye, at, std::abs(eye.y - at.y) > 4.9f && eye.x == at.x && eye.z == at.z ? Vec3{0, 0, -1} : Vec3{0, 1, 0});
        camera.projection = perspective(radians(60), 1.0f, 0.1f, 100.0f);
    }
    std::vector<std::uint32_t> draw(const RenderScene& scene) {
        auto cmd = device->begin_commands();
        renderer->render(cmd, target, camera, scene);
        cmd.transition(target, gpu::ResourceState::TransferSrc);
        cmd.copy_texture_to_buffer(target, readback);
        device->wait(device->submit(cmd));
        std::vector<std::uint32_t> pixels(size * size);
        device->read_buffer(readback, pixels.data(), pixels.size() * 4);
        return pixels;
    }
    /// Pixel where a world point lands on screen.
    std::uint32_t at(const std::vector<std::uint32_t>& pixels, Vec3 world) const {
        const Vec4 clip = camera.projection * camera.view * Vec4{world.x, world.y, world.z, 1};
        const float x = (clip.x / clip.w * 0.5f + 0.5f) * size;
        const float y = (clip.y / clip.w * 0.5f + 0.5f) * size;
        const auto px = static_cast<std::uint32_t>(std::clamp(x, 0.0f, size - 1.0f));
        const auto py = static_cast<std::uint32_t>(std::clamp(y, 0.0f, size - 1.0f));
        return pixels[py * size + px];
    }
    static std::uint32_t center(const std::vector<std::uint32_t>& p) { return p[(size / 2) * size + size / 2]; }
};

const DirectionalLight front_sun{.direction = {0, 0, -1}, .intensity = 2.0f, .shadows = false};

} // namespace

TEST_CASE("primitive meshes have consistent geometry") {
    const MeshData cube = make_cube();
    CHECK(cube.vertices.size() == 24);
    CHECK(cube.indices.size() == 36);
    CHECK(make_plane(10.0f).indices.size() == 6);
    const MeshData sphere = make_sphere(16, 8);
    for (auto i : sphere.indices) CHECK(i < sphere.vertices.size());
}

TEST_CASE("a lit mesh is drawn over the sky") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    const std::vector<DrawItem> items{{.mesh = cube, .color = {1, 0, 0, 1}, .roughness = 1.0f}};
    auto pixels = f.draw({.meshes = items, .sun = front_sun});
    const Rgb c = rgb(Fixture::center(pixels));
    CHECK(c.r > 150);
    CHECK(c.g < 60);
    const Rgb sky = rgb(pixels[0]); // top-left: sky, bluish
    CHECK(sky.b > sky.r);
    CHECK(f.renderer->stats().passes >= 3);
}

TEST_CASE("depth test keeps the nearer mesh in front") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    const std::vector<DrawItem> items{{.mesh = cube, .transform = translate({0, 0, 1}), .color = {0, 1, 0, 1}, .roughness = 1.0f},
                                      {.mesh = cube, .transform = translate({0, 0, -1}), .color = {1, 0, 0, 1}, .roughness = 1.0f}};
    const Rgb c = rgb(Fixture::center(f.draw({.meshes = items, .sun = front_sun})));
    CHECK(c.g > 150);
    CHECK(c.r < 60);
}

TEST_CASE("many instances of one mesh are one draw call") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    std::vector<DrawItem> items;
    for (int i = 0; i < 1000; ++i) items.push_back({.mesh = cube, .transform = translate({0, 0, -static_cast<float>(i)})});
    f.draw({.meshes = items, .sun = front_sun});
    CHECK(f.renderer->stats().draw_calls == 1);
    CHECK(f.renderer->stats().instances == 1000);
}

TEST_CASE("draw items with destroyed meshes are skipped") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    f.renderer->destroy(cube);
    const std::vector<DrawItem> items{{.mesh = cube, .color = {1, 0, 0, 1}}};
    const Rgb c = rgb(Fixture::center(f.draw({.meshes = items, .sun = front_sun})));
    CHECK(c.b > c.r); // sky, not a red cube
}

TEST_CASE("an SDF sphere is raymarched into the image") {
    Fixture f;
    if (!f.device) return;
    const std::vector<SdfShape> shapes{{.primitive = SdfPrimitive::Sphere, .size = {1, 0, 0, 0}, .color = {1, 0, 0, 1}, .roughness = 1.0f}};
    const Rgb c = rgb(Fixture::center(f.draw({.sdf = shapes, .sun = front_sun})));
    CHECK(c.r > 150);
    CHECK(c.g < 60);
    CHECK(f.renderer->stats().sdf_shapes == 1);
}

TEST_CASE("meshes and SDF shapes are depth-composited") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    SUBCASE("mesh in front") {
        const std::vector<DrawItem> items{{.mesh = cube, .transform = translate({0, 0, 1.5f}), .color = {0, 1, 0, 1}}};
        const std::vector<SdfShape> shapes{{.size = {1, 0, 0, 0}, .color = {1, 0, 0, 1}}};
        const Rgb c = rgb(Fixture::center(f.draw({.meshes = items, .sdf = shapes, .sun = front_sun})));
        CHECK(c.g > c.r);
    }
    SUBCASE("SDF in front") {
        const std::vector<DrawItem> items{{.mesh = cube, .transform = translate({0, 0, -2}), .color = {0, 1, 0, 1}}};
        const std::vector<SdfShape> shapes{{.position = {0, 0, 1}, .size = {1, 0, 0, 0}, .color = {1, 0, 0, 1}}};
        const Rgb c = rgb(Fixture::center(f.draw({.meshes = items, .sdf = shapes, .sun = front_sun})));
        CHECK(c.r > c.g);
    }
}

TEST_CASE("SDF subtraction carves a hole") {
    Fixture f;
    if (!f.device) return;
    // A big box with a sphere subtracted at the front-center: the center pixel sees deeper in,
    // so the box face at the center is gone (we look into the carved cavity).
    const std::vector<SdfShape> solid{{.primitive = SdfPrimitive::Box, .operation = SdfOperation::Union,
                                       .size = {1, 1, 1, 0}, .color = {1, 1, 1, 1}}};
    const std::vector<SdfShape> carved{solid[0], {.primitive = SdfPrimitive::Sphere, .operation = SdfOperation::Subtract,
                                                  .position = {0, 0, 1.2f}, .size = {0.6f, 0, 0, 0}}};
    const int solid_l = luminance(Fixture::center(f.draw({.sdf = solid, .sun = front_sun})));
    const int carved_l = luminance(Fixture::center(f.draw({.sdf = carved, .sun = front_sun})));
    CHECK(carved_l != solid_l);
}

TEST_CASE("point lights light the scene") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    const std::vector<DrawItem> items{{.mesh = cube, .color = {1, 1, 1, 1}}};
    const DirectionalLight dark{.intensity = 0.0f, .shadows = false};
    const Environment no_ambient{.ambient = 0.0f};
    const Rgb unlit = rgb(Fixture::center(f.draw({.meshes = items, .sun = dark, .environment = no_ambient})));
    const std::vector<PointLight> lights{{.position = {0, 0, 2}, .color = {1, 0, 0}, .intensity = 20, .radius = 10}};
    const Rgb lit = rgb(Fixture::center(f.draw({.meshes = items, .lights = lights, .sun = dark, .environment = no_ambient})));
    CHECK(unlit.r < 20);
    CHECK(lit.r > 100);
    CHECK(lit.g < 30);
}

TEST_CASE("the sun casts shadows from meshes") {
    Fixture f;
    if (!f.device) return;
    MeshHandle cube = f.renderer->create_mesh(make_cube());
    MeshHandle plane = f.renderer->create_mesh(make_plane(1.0f));
    f.look_from({0, 8, 6}, {0, 0, 0});
    const std::vector<DrawItem> items{{.mesh = plane, .transform = scale({20, 1, 20})},
                                      {.mesh = cube, .transform = translate({0, 1, 0})}};
    const DirectionalLight sun{.direction = {1, -1, 0}, .intensity = 3.0f, .shadows = true};
    auto pixels = f.draw({.meshes = items, .sun = sun});
    const int shadowed = luminance(f.at(pixels, {1.3f, 0, 0}));  // behind the cube, away from the sun
    const int lit = luminance(f.at(pixels, {-2.5f, 0, 0}));      // open ground
    CHECK(shadowed < lit - 60);
    CHECK(f.renderer->stats().shadow_draw_calls == 2);
}
